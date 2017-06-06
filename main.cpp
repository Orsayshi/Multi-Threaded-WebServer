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

int s, sock, ch, server, done, bytes, aflg;
int soctype = SOCK_STREAM;
int queuing_time = 60;
int thread_num = 4;
char *host = NULL;
char *port = NULL;

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
    if (!server && (host == NULL || port == NULL))
        usage();
    if (server && host != NULL)
        usage();
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