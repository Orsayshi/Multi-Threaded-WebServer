//
// Created by  Gmyth on 17/6/5.
//
#define	BUF_LEN	8192

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<ctype.h>
#include	<sys/types.h>
#include	<sys/socket.h>
#include	<netdb.h>
#include    <getopt.h>
#include    <string>
#include	<netinet/in.h>
#include	<inttypes.h>

char *progname;
char buf[BUF_LEN];

void usage();
int setup_client();
int setup_server();
int req_parser(char buffer[]);

int s, sock, ch, server, done, bytes, aflg;
int soctype = SOCK_STREAM;
int queuing_time = 60;
int thread_num = 4;
char *host = NULL;
char *port = NULL;

struct request
{
    char time_arrival[250];
    char *serverName;
    int  content_size;
    char *file_dir;
    char *content_type;
    char last_modified[250];
};

// all flags
bool debugging = false;
bool NOT_FCFS = false;



extern char *optarg;
extern int optind;

int
main(int argc,char *argv[])
{
    fd_set ready;
    struct sockaddr_in msgfrom;
//    int msgsize;
    socklen_t msgsize;
    union {
        uint32_t addr;
        char bytes[4];
    } fromaddr;

    if ((progname = rindex(argv[0], '/')) == NULL)
        progname = argv[0];
    else
        progname++;
    while ((ch = getopt(argc, argv, "adsp:h:")) != -1)
        switch(ch) {
            case 'd':
                // entering the debug mode
                debugging = true;
                break;
            case 't':
                // Set the queuing time to time seconds. The default should be 60 seconds

                // make sure the arg is an integer
                queuing_time = std::stoi(optarg);
                break;
            case 'p':
                // Listen on the given port. If not provided, myhttpd will listen on port 8080.
                port = optarg;
                break;
            case 'n':
                /*
                 * Set number of threads waiting ready in the execution thread pool to threadnum.
                 * The default should be 4 execution threads.
                 */
                // syntax check needed here
                thread_num = std::stoi(optarg);
                break;
            case 's':
                // Set the scheduling policy. It can be either FCFS or SJF. The default will be FCFS.
                char* mode;
                mode = optarg;
                if(strcmp (mode,"SJF") == 0){
                    NOT_FCFS = true;
                }
                break;
            case 'h':
                // print usage with all the option and exit
                usage();
                exit(1);
                break;
            case 'l':
                // Log all requests to the given file.
                //function needed
                break;
            case 'r':
                // Set the root directory for the http server to dir.
                break;
            default:
                usage();
        }
    argc -= optind; // reduces the argument number by optind
    if (argc != 0)
        usage();
//    if (!server && (host == NULL || port == NULL))
//        usage();
//    if (server && host != NULL)
//        usage();
/*
 * Create socket on local host.
 */
    if ((s = socket(AF_INET, soctype, 0)) < 0) {
        perror("socket");
        exit(1);
    }
//    if (!server)
//        sock = setup_client();
//    else
    sock = setup_server();
/*
 * Set up select(2) on both socket and terminal, anything that comes
 * in on socket goes to terminal, anything that gets typed on terminal
 * goes out socket...
 */
    while (!done) {
        FD_ZERO(&ready);
        FD_SET(sock, &ready);
        FD_SET(fileno(stdin), &ready);
        if (select((sock + 1), &ready, 0, 0, 0) < 0) {
            perror("select");
            exit(1);
        }
        if (FD_ISSET(fileno(stdin), &ready)) {
            if ((bytes = read(fileno(stdin), buf, BUF_LEN)) <= 0)
                done++;
            send(sock, buf, bytes, 0);
        }
        msgsize = sizeof(msgfrom);
        if (FD_ISSET(sock, &ready)) {
            if ((bytes = recvfrom(sock, buf, BUF_LEN, 0, (struct sockaddr *)&msgfrom, &msgsize)) <= 0) {
                done++;
            } else if (aflg) {
                fromaddr.addr = ntohl(msgfrom.sin_addr.s_addr);
                fprintf(stderr, "%d.%d.%d.%d: ", 0xff & (unsigned int)fromaddr.bytes[0],
                        0xff & (unsigned int)fromaddr.bytes[1],
                        0xff & (unsigned int)fromaddr.bytes[2],
                        0xff & (unsigned int)fromaddr.bytes[3]);
            }
            write(fileno(stdout), buf, bytes);
            buf[strlen(buf) - 1] = '\0';
            //buf[strlen(buf)] = '\n';
            if(strcmp(buf,"exit")==0){
                exit(1);
            }
            req_parser(buf);

            exit(1);
        }
    }
    return(0);
}

/*
 * setup_server() - set up socket for mode of soc running as a server.
 */

int
setup_server() {
    struct sockaddr_in serv, remote;
    struct servent *se;
    //    int newsock, len;
    socklen_t newsock, len;
    len = sizeof(remote);
    memset((void *)&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    if (port == NULL)
        serv.sin_port = htons(0);
    else if (isdigit(*port))
        serv.sin_port = htons(atoi(port));
    else {
        if ((se = getservbyname(port, (char *)NULL)) < (struct servent *) 0) {
            perror(port);
            exit(1);
        }
        serv.sin_port = se->s_port;
    }
    if (bind(s, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("bind");
        exit(1);
    }
    if (getsockname(s, (struct sockaddr *) &remote, &len) < 0) {
        perror("getsockname");
        exit(1);
    }
    fprintf(stderr, "Port number is %d\n", ntohs(remote.sin_port));
    listen(s, 1);
    newsock = s;
    if (soctype == SOCK_STREAM) {
        fprintf(stderr, "Entering accept() waiting for connection.\n");
        newsock = accept(s, (struct sockaddr *) &remote, &len);
    }
    return(newsock);
}


/*
 * parse the incoming request, maintian information
 *
 */
 int
 req_parser(char buffer[]){
    printf("New Request detected, start parsing process....");
    FILE *in;
    char *Request_type = strtok(buffer," ");
    char *dir = strtok(NULL," ");
    char *def = "/Users/gmyth/Desktop/CSE_421_WebServer"; // this one need to change, i will pust this into config file
    char *type;
    time_t now;
    time(&now);
    struct tm * Current=localtime(&now);
    //printf("\nck 1");
    if(Request_type == NULL){
        Request_type = strtok(NULL," ");
    }else{
       // printf("\nRequest_type: \"%s\"",Request_type);
        if(strcmp(Request_type,"GET")==0 || strcmp(Request_type,"HEAD")==0){
           // printf("\nck 2");
            char *temp = (char *)malloc(strlen(dir)+strlen(def)+1);
            strcpy(temp,def);
            strcat(temp,dir);
            //printf("\ntemp: \"%s\"",temp);
            in = fopen(temp,"r");//in read mode
            if(in == NULL){
                printf("Unable to open file");
                return 2;//no file
            }
            strtok(dir,".");
            type =strtok(NULL,".");
            //printf("\ncame to here");
            //printf("\ntype: \"%s\"",type);
            if(type==NULL){
                printf("unsuportted file type");
                return 3; // unsuportted file
            }
            if(strcmp(type,"html")==0){
                type = "text/html";
            }else if(strcmp(type,"gif")==0){
                type = "image/gif";
            }else{
                printf("unsuportted file type");
                return 3;//no file
            }

            fseek(in, 0, SEEK_END); // seek to end of file
            int size = ftell(in);
            char current_ts[250];
            //printf("\ncontent size:\"%d\"", size);
            strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
            //printf("\ntime stamp:\"%s\"", current_ts);
            struct request new_request;
            new_request.content_size = size;
            new_request.content_type = type;
            new_request.file_dir = temp;
            strcpy(new_request.last_modified ,current_ts);
            new_request.serverName="Hello world muilti-thread server";
            strcpy(new_request.time_arrival , current_ts);
//            free(in);
//            free(type);
//            free(Request_type);
//            free(dir);
//            free(def);
//            free(temp);
            printf("\nRequest content_size: \"%d\"",new_request.content_size);
            printf("\nRequest content_type: \"%s\"",new_request.content_type);
            printf("\nRequest file_dir: \"%s\"",new_request.file_dir);
            printf("\nRequest last_modified: \"%s\"",new_request.last_modified);
            printf("\nRequest serverName: \"%s\"",new_request.serverName);
            printf("\nRequest time_arrival: \"%s\"",new_request.time_arrival);
        }else{
            // wrong request type
            return 1;// 1 is the err code for req_parser can't find correct tyoe
        }
    }
 }
/*
 * usage - print usage string and exit
 */

void
usage()
{
    // change this to new usage
    fprintf(stderr, "usage: %s -h host -p port\n", progname);
    fprintf(stderr, "usage: %s -s [-p port]\n", progname);
    exit(1);
}