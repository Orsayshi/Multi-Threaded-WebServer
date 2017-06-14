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
<<<<<<< HEAD
#include  <getopt.h>
=======
#include    <getopt.h>
#include    <time.h>
#include  <errno.h>
//#include    <sys/sendfile.h>
>>>>>>> 5a89b0c69ce45463613af60cf58ea7cdfdf788a1
#include	<netinet/in.h>
#include	<inttypes.h>
#include  <unistd.h>


char *progname;
char buf[BUF_LEN];

void usage();
int setup_client();
int setup_server();
int req_parser(char buffer[]);
int request_handler(struct request rq);

int s, sock, ch, server, done, bytes, aflg;
int soctype = SOCK_STREAM;
int queuing_time = 60;
int thread_num = 4;
char *host = NULL;
char *port = NULL;

struct request
{
    char *request_type;
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
                queuing_time = atoi(optarg);
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
                thread_num = atoi(optarg);
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
            buf[bytes - 1] = '\0';
           // printf("current:--------%s---------\n",buf);
            if(strcasecmp((char *)buf,"exit")==0){
                exit(1);
            }
            req_parser(buf);
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
    if(strcmp(buffer,"exit")==0){
        exit(1);
    }
    FILE *in;
    char *Request_type = strtok(buffer," ");
    char *dir = strtok(NULL," ");
    char *def = (char*)"/Users/gmyth/Desktop/CSE_421_WebServer"; // this one need to change, i will pust this into config file
    char *type;
    time_t now;
    time(&now);
    struct tm * Current=localtime(&now);
    //printf("\nck 1");
    if(Request_type == NULL){
        Request_type = strtok(NULL," ");
    }
    if(Request_type == NULL){
        write(sock,"\nunsuportted request type\n",26);
        return 5;
    }
    else{
       // printf("\nRequest_type: \"%s\"",Request_type);
        if(strcmp(Request_type,"GET")==0 || strcmp(Request_type,"HEAD")==0){
           // printf("\nck 2");
            char *temp = (char *)malloc(strlen(dir)+strlen(def)+1);
            strcpy(temp,def);
            strcat(temp,dir);
            //printf("\ntemp: \"%s\"",temp);
            in = fopen(temp,"r");//in read mode
            if(in == NULL){
                write(sock,"\nUnable to open file\n",21);
                return 2;//no file
            }
            strtok(dir,".");
            type =strtok(NULL,".");
            //printf("\ncame to here");
            //printf("\ntype: \"%s\"",type);
            if(type==NULL){
                write(sock,"\nunsuportted file type\n",23);
                return 3; // unsuportted file
            }
            if(strcmp(type,"html")==0){
                type = (char*)"text/html";
            }else if(strcmp(type,"gif")==0){
                type = (char*)"image/gif";
            }else{
                write(sock,"\nunsuportted file type\n",23);
                return 3;//no file
            }
            fseek(in, 0, SEEK_END); // seek to end of file
            int size = ftell(in);
            fclose(in);
            char current_ts[250];
            //printf("\ncontent size:\"%d\"", size);
            strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
            //printf("\ntime stamp:\"%s\"", current_ts);
            struct request new_request;
            new_request.content_size = size;
            new_request.content_type = type;
            new_request.file_dir = temp;
            strcpy(new_request.last_modified ,current_ts);
            new_request.serverName=(char*)"Hello world muilti-thread server";
            strcpy(new_request.time_arrival , current_ts);
            new_request.request_type = Request_type;
//            free(in);
//            free(type);
//            free(Request_type);
//            free(dir);
//            free(def);
//            free(temp);
            printf("\nRequest_type: \"%s\"",new_request.request_type);
            printf("\nRequest content_size: \"%d\"",new_request.content_size);
            printf("\nRequest content_type: \"%s\"",new_request.content_type);
            printf("\nRequest file_dir: \"%s\"",new_request.file_dir);
            printf("\nRequest last_modified: \"%s\"",new_request.last_modified);
            printf("\nRequest serverName: \"%s\"",new_request.serverName);
            printf("\nRequest time_arrival: \"%s\"",new_request.time_arrival);
           // test code, call response in here
            request_handler(new_request);
        }else{
            // wrong request type
            write(sock,"\nwrong request type\n",20);
            return 1;// 1 is the err code for req_parser can't find correct tyoe
        }
    }
    return 0;
 }

int request_handler(struct request rq){
    //int status = 400;
    char buf [200];
    time_t now;
    time(&now);
    struct tm * Current=localtime(&now);
    char current_ts[250];
    //printf("\ncontent size:\"%d\"", size);
    strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
    if(strcmp(rq.request_type,"GET")==0){
        // GET response
        FILE* in;
        //get file by directory, since the directory is already checked in parser function, no need re-check here
        in = fopen(rq.file_dir,"r");
        char length_buffer[20];
        strcpy(rq.last_modified ,current_ts);
        sprintf(length_buffer,"%d",rq.content_size); // convert int to char
        write(sock,"\n",1);
        write(sock,"Hello world muilti-thread server\n",33);
        write(sock,"HTTP/1.1 200 OK\n",16);
        write(sock,"Last Modified: ",15);
        write(sock,rq.last_modified,strlen(rq.last_modified));
        write(sock,"\nContent-Type: text/html\n",24);
        write(sock,"\n",1);
        write(sock,"Content Length: ",16);
        write(sock,length_buffer,strlen(length_buffer));
        write(sock,"\n",1);
        write(sock,"------------------------------\n",31);
        //sendfile(sock,fileno(in),NULL,sizeof(buf)); one for linux
        int check = sendfile(fileno(in),sock,0,(off_t *)length_buffer,NULL,0); // this one only work under mac OS
        if(check!=0){
            printf("s:%d\n",errno);
            printf("Oh dear, something went wrong with sendfile()! %s\n", strerror(errno));
        }
        write(sock,"\n------------------------------\n",32);
        fclose(in);
        return 0;
    }else if(strcmp(rq.request_type,"HEAD")==0){
        // HEAD response
        FILE* in;
        //get file by directory, since the directory is already checked in parser function, no need re-check here
        in = fopen(rq.file_dir,"r");
        char length_buffer[20];
        strcpy(rq.last_modified ,current_ts);
        sprintf(length_buffer,"%d",rq.content_size);
        write(sock,"\n",1);
        write(sock,"Hello world muilti-thread server\n",33);
        write(sock,"HTTP/1.1 200 OK\n",16);
        write(sock,"Date: ",6);
        write(sock,rq.time_arrival,strlen(rq.time_arrival));
        write(sock,"\nLast Modified: ",16);
        write(sock,rq.last_modified,strlen(rq.last_modified));
        write(sock,"\nContent-Type: text/html\n",24);
        write(sock,"\n",1);
        write(sock,"Content Length: ",16);
        write(sock,length_buffer,strlen(length_buffer));
        write(sock,"\n",1);
        fclose(in);
        return 0;
    }
    return -1;
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