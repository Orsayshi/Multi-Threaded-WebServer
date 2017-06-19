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
#include    <time.h>
#include    <errno.h>
#include    <pthread.h>
#include    <semaphore.h>
//#include    <sys/sendfile.h>
#include	<netinet/in.h>
#include	<inttypes.h>


char *progname;
char buf[BUF_LEN];

void usage();
int setup_client();
int setup_server();
void *scheduling(void *);
void *servicing(void *);
void *listenning(void *);
void enqueue(struct request *rq);
void get_shortest_job();
int queue_size();
void file_log(char *info);
void send_err_feedback();
void queue_err_feedback(struct invalid_request *rq);
int req_parser(char buffer[], char ip[]);
int request_handler(struct request *rq);

int s, sock, ch, server, done, bytes, aflg;
int soctype = SOCK_STREAM;
int queuing_time = 60;
int threads = 4;
char *mode;
char *log_file = NULL;
char dir_buf[1000];
char *host = NULL;
char *port = NULL;
char *root = getcwd(dir_buf,1000);
sem_t *sem;
struct request *head = NULL;
struct invalid_request *errhead = NULL;
pthread_t scheduler_id,service_id,listen_id;
struct request *ready_rq = NULL;

pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scheduler_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t output_lock=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

struct request
{
    char ip[300];
    char *request_type;
    char time_arrival[250];
    char *serverName;
    int  content_size;
    char content[601];
    char file_dir[601];
    char *content_type;
    char last_modified[250];
    struct request *tail;
};

struct invalid_request
{
    char ip_address[300];
    char time_arrival[250];
    char *serverName;
    char *msg;
    char content[300];
    char last_modified[250];
    struct invalid_request *tail;
};

// all flags
bool debugging = false;
bool NOT_FCFS = false;



extern char *optarg;
extern int optind;

int
main(int argc,char *argv[]) {
    if ((progname = rindex(argv[0], '/')) == NULL)
        progname = argv[0];
    else
        progname++;
    while ((ch = getopt(argc, argv, "dt:p:n:s:hr:l:")) != -1)
        switch (ch) {
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
                threads = atoi(optarg);
                break;
            case 's':
                // Set the scheduling policy. It can be either FCFS or SJF. The default will be FCFS.
                mode = optarg;
                if (strcmp(mode, "SJF") == 0) {
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
                log_file = optarg;
                if(log_file[0]!='/'){
                    log_file = log_file -1;
                }
                log_file[0]='/';
                char temp[1000];
                strcpy(temp,root);
                strcat(temp,log_file);
                strcpy(log_file,temp);
                break;
            case 'r':
                // Set the root directory for the http server to dir.
                root = optarg;
                if(chdir(root)<0){
                    fprintf(stderr,"Unvalid working directory %s\n",root);
                    root = getcwd(dir_buf,1000);
                }
                printf("Current working dircetory is %s\n",getcwd(dir_buf,1000));
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
 * Set up service thread and scheduler thread
 */
    //sem_init(&sem,0,threads);
    sem = sem_open("service",0,threads);
    pthread_t services[threads];
    for (int i = 0; i < threads; i++) {
        pthread_create(&services[i], NULL, &servicing, NULL);
        // create threads pool
    }
    pthread_create(&scheduler_id, NULL, scheduling, &NOT_FCFS);
    pthread_create(&listen_id, NULL, listenning, NULL);
    pthread_join(listen_id, NULL);
    pthread_join(scheduler_id, NULL);
    printf("\nhit!");
    exit(0);
}
void *listenning(void *nulptr){
/*
 * Set up select(2) on both socket and terminal, anything that comes
 * in on socket goes to terminal, anything that gets typed on terminal
 * goes out socket...
 */
    (void) nulptr;
    fd_set ready;
    struct sockaddr_in msgfrom;
//    int msgsize;
    socklen_t msgsize;
    union {
        uint32_t addr;
        char bytes[4];
    } fromaddr;
    //printf("listenning\n");
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
            //cwrite(fileno(stdout), buf, bytes);
           // buf[bytes - 1] = '\0';
           // printf("current:--------%s---------\n",buf);
//            if(strcasecmp((char *)buf,"exit")==0){
//                exit(1);
//            }
            char ip[300];
            fromaddr.addr = ntohl(msgfrom.sin_addr.s_addr);
            sprintf(ip, "%d.%d.%d.%d: ", 0xff & (unsigned int)fromaddr.bytes[0],
                    0xff & (unsigned int)fromaddr.bytes[1],
                    0xff & (unsigned int)fromaddr.bytes[2],
                    0xff & (unsigned int)fromaddr.bytes[3]);
            req_parser(buf,ip);
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
 * servicing method, working on request
 *
 */
void *servicing(void * pointer){
    (void) pointer;
     while(1){
         pthread_mutex_lock(&scheduler_lock);
         pthread_cond_wait(&cv,&scheduler_lock);
         struct request *input = ready_rq;
         pthread_mutex_unlock(&scheduler_lock);
         pthread_mutex_lock(&output_lock);
         request_handler(input);
         printf("\nservice finished\n");
         pthread_mutex_unlock(&output_lock);
         sem_post(sem);
     }
}
/*
 * schduling method, assign quest to worker
 *
 */
void *scheduling(void *no_FCFS){
    bool *SJF = (bool *)no_FCFS;
    if(head == NULL){
        printf("\nNo avaliable request detected...\n");
    }
    while(1) {
        sleep(queuing_time);
        if(head == NULL){
            printf("\nNo avaliable request detected...\n");
        }
        else {
            printf("\nwaiting finished, start scheduled...\n");
        }
        if(!SJF) {
            while (head != NULL) {
                sem_wait(sem);
                ready_rq = NULL;
                pthread_mutex_lock(&scheduler_lock);
                pthread_mutex_lock(&queue_lock);
                time_t now;
                time(&now);
                struct tm *Current = localtime(&now);
                char current_ts[250];
                //printf("\ncontent size:\"%d\"", size);
                strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
                ready_rq = head;
                strcpy(ready_rq->last_modified, current_ts);
                // ready holds the scheduled request ready to assign to service
                head = head->tail;
                pthread_mutex_unlock(&queue_lock);
                pthread_cond_signal(&cv);
                pthread_mutex_unlock(&scheduler_lock);
                sleep(1);
            }
        }else{
            while (head != NULL){
                sem_wait(sem);
                ready_rq = NULL;
                pthread_mutex_lock(&scheduler_lock);
                pthread_mutex_lock(&queue_lock);
                get_shortest_job();
                // ready holds the scheduled request ready to assign to service
                //head = head->tail;
                pthread_mutex_unlock(&queue_lock);
                pthread_cond_signal(&cv);
                pthread_mutex_unlock(&scheduler_lock);
                sleep(1);
            }
        }
        send_err_feedback();
       printf("\nQueue gets empty...\n");
   }
}
/*
 * Request Enqueue;
 *
 */
void enqueue(struct request *rq){
    if(head == NULL ){
        head = rq;
    }else{
        struct request *temp = head;
        while(NULL!=temp->tail){
            temp = temp->tail;
        }
        temp->tail = rq;
    }
    printf("\nnew request added, now size is: %d \n",queue_size());
}
/*
 * parse the incoming request, maintian information
 *
 */
int
req_parser(char buffer[], char ip[]){
    printf("New Request detected, start parsing process....");
    FILE *in;
    char content[300];
    strcpy(content,buffer);
    char *Request_type = strtok(buffer," ");
    char *dir = strtok(NULL," ");
    char temp[1300];
    char dir_1[1300];
    strcpy(temp,root);
    if(dir != NULL){
        strcpy(dir_1,dir);
        if(dir_1[0]=='~'){
            dir = dir +1;// move one bit
            strcpy(dir_1,dir);
            strncat(temp,dir_1,strlen(dir_1)+strlen(temp)+1);
        }else{
            strcpy(temp,dir);
        }
    }

   // printf("\nstring: %s",temp);
    char *type;
    time_t now;
    time(&now);
    struct tm * Current=localtime(&now);
    //printf("\nck 1");
    if(Request_type == NULL){
        Request_type = strtok(NULL," ");
    }
    if(Request_type == NULL){
        struct invalid_request *current;
        char current_ts[250];
        //printf("\ncontent size:\"%d\"", size);
        strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
        current = (struct invalid_request *)malloc(sizeof(struct invalid_request));
        strcpy(current->ip_address,ip);
        strcpy(current->content,content);
        strcpy(current->time_arrival,current_ts);
        strcpy(current->last_modified,current_ts);
        current->serverName=(char*)"Hello world muilti-thread server";
        current->msg = (char*)"\nUnsuportted request type\n";
        current->tail = NULL;
        queue_err_feedback(current);
        //write(sock,"\nunsuportted request type\n",26);
        return 5;
    }
    else{
        // printf("\nRequest_type: \"%s\"",Request_type);
        if(strcmp(Request_type,"GET")==0 || strcmp(Request_type,"HEAD")==0){
            // printf("\nck 2");
//            char *temp = (char *)malloc(strlen(dir)+strlen(def)+1);
//            strcpy(temp,def);
//            strcat(temp,dir);
            //printf("\ntemp: \"%s\"",temp);
            in = fopen(temp,"r");//in read mode
            if(in == NULL){
                struct invalid_request *current;
                char current_ts[250];
                //printf("\ncontent size:\"%d\"", size);
                strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
                current = (struct invalid_request *)malloc(sizeof(struct invalid_request));
                strcpy(current->ip_address,ip);
                strcpy(current->time_arrival,current_ts);
                strcpy(current->content,content);
                strcpy(current->last_modified,current_ts);
                current->serverName=(char*)"Hello world muilti-thread server";
                current->msg = (char*)"\nUnable to open file\n";
                current->tail = NULL;
                queue_err_feedback(current);
                return 2;//no file
            }
            strtok(dir,".");
            type =strtok(NULL,".");
            //printf("\ncame to here");
            //printf("\ntype: \"%s\"",type);
            if(type==NULL){
                //write(sock,"\nunsuportted file type\n",23);
                struct invalid_request *current;
                char current_ts[250];
                //printf("\ncontent size:\"%d\"", size);
                strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
                current = (struct invalid_request *)malloc(sizeof(struct invalid_request));
                strcpy(current->ip_address,ip);
                strcpy(current->time_arrival,current_ts);
                strcpy(current->content,content);
                strcpy(current->last_modified,current_ts);
                current->serverName=(char*)"Hello world muilti-thread server";
                current->msg = (char*)"\nCan't read file type from request\n";
                current->tail = NULL;
                queue_err_feedback(current);
                return 3; // unsuportted file
            }
            if(strcmp(type,"html")==0){
                type = (char*)"text/html";
            }else if(strcmp(type,"gif")==0){
                type = (char*)"image/gif";
            }else{
                //write(sock,"\nunsuportted file type\n",23);
                struct invalid_request *current;
                char current_ts[250];
                //printf("\ncontent size:\"%d\"", size);
                strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
                current = (struct invalid_request *)malloc(sizeof(struct invalid_request));
                strcpy(current->ip_address,ip);
                strcpy(current->time_arrival,current_ts);
                strcpy(current->content,content);
                strcpy(current->last_modified,current_ts);
                current->serverName=(char*)"Hello world muilti-thread server";
                current->msg = (char*)"\nFile type is not supported...\n";
                current->tail = NULL;
                queue_err_feedback(current);
                return 3;//no file
            }
            fseek(in, 0, SEEK_END); // seek to end of file
            int size = ftell(in);
            fclose(in);
            char current_ts[250];
            //printf("\ncontent size:\"%d\"", size);
            strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
            //printf("\ntime stamp:\"%s\"", current_ts);
            struct request *new_request;
            new_request = (struct request *)malloc(sizeof(struct request));
            new_request->content_size = size;
            new_request->content_type = type;
            strcpy(new_request->ip,ip);
            strcpy(new_request->content,content);
            strcpy(new_request->file_dir,temp);
            strcpy(new_request->last_modified ,current_ts);
            new_request->serverName=(char*)"Hello world muilti-thread server";
            strcpy(new_request->time_arrival , current_ts);
            new_request->request_type = Request_type;
//            free(in);
//            free(type);
//            free(Request_type);
//            free(dir);
//            free(def);
//            free(temp);
            printf("\nRequest_type: \"%s\"",new_request->request_type);
            printf("\nRequest content_size: \"%d\"",new_request->content_size);
            printf("\nRequest content_type: \"%s\"",new_request->content_type);
            printf("\nRequest file_dir: \"%s\"",new_request->file_dir);
            printf("\nRequest last_modified: \"%s\"",new_request->last_modified);
            printf("\nRequest serverName: \"%s\"",new_request->serverName);
            printf("\nRequest time_arrival: \"%s\"",new_request->time_arrival);
            printf("\n");
            enqueue(new_request);
        }else{
            // wrong request type
            struct invalid_request *current;
            char current_ts[250];
            //printf("\ncontent size:\"%d\"", size);
            strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
            current = (struct invalid_request *)malloc(sizeof(struct invalid_request));
            strcpy(current->ip_address,ip);
            strcpy(current->time_arrival,current_ts);
            strcpy(current->content,content);
            strcpy(current->time_arrival,current_ts);
            strcpy(current->last_modified,current_ts);
            current->serverName=(char*)"Hello world muilti-thread server";
            current->msg = (char*)"\nOnly GET and HEAD request can be accepted...\n";
            current->tail = NULL;
            queue_err_feedback(current);
            return 1;// 1 is the err code for req_parser can't find correct tyoe
        }
    }
    return 0;
}

int request_handler(struct request *rq){
    //int status = 400;
    char buf [200];
    time_t now;
    time(&now);
    struct tm * Current=localtime(&now);
    char current_ts[250];
    //printf("\ncontent size:\"%d\"", size);
    strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);
    if(rq == NULL){
        return -1;
    }
    if(strcmp(rq->request_type,"GET")==0){
        // GET response
        FILE* in;
        //get file by directory, since the directory is already checked in parser function, no need re-check here
        in = fopen(rq->file_dir,"r");
        char length_buffer[20];
        strcpy(rq->last_modified ,current_ts);
        sprintf(length_buffer,"%d",rq->content_size); // convert int to char
        write(sock,"\n",1);
        write(sock,"Hello world muilti-thread server\n",33);
        write(sock,"HTTP/1.1 200 OK\n",16);
        write(sock,"Last Modified: ",15);
        write(sock,rq->last_modified,strlen(rq->last_modified));
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
        char log_buf[3000];
        sprintf(log_buf,"%s - [%s] [%s] \"%s\" %d %d",rq->ip,rq->time_arrival,rq->last_modified,rq->content,200,rq->content_size);
        file_log(log_buf);
        if(debugging){
            fprintf(stderr,"%s",log_buf);
            fprintf(stderr,"\n");
        }

        fclose(in);
        if(rq != NULL) {
            free(rq);
        }
        return 0;
    }else if(strcmp(rq->request_type,"HEAD")==0){
        // HEAD response
        FILE* in;
        //get file by directory, since the directory is already checked in parser function, no need re-check here
        in = fopen(rq->file_dir,"r");
        char length_buffer[20];
        strcpy(rq->last_modified ,current_ts);
        sprintf(length_buffer,"%d",rq->content_size);
        write(sock,"\n",1);
        write(sock,"Hello world muilti-thread server\n",33);
        write(sock,"HTTP/1.1 200 OK\n",16);
        write(sock,"Date: ",6);
        write(sock,rq->time_arrival,strlen(rq->time_arrival));
        write(sock,"\nLast Modified: ",16);
        write(sock,rq->last_modified,strlen(rq->last_modified));
        write(sock,"\nContent-Type: text/html\n",24);
        write(sock,"\n",1);
        write(sock,"Content Length: ",16);
        write(sock,length_buffer,strlen(length_buffer));
        write(sock,"\n",1);
        char log_buf[3000];
        sprintf(log_buf,"%s - [%s] [%s] \"%s\" %d %d",rq->ip,rq->time_arrival,rq->last_modified,rq->content,200,rq->content_size);
        file_log(log_buf);
        if(debugging){
            fprintf(stderr,"%s",log_buf);
            fprintf(stderr,"\n");
        }
        fclose(in);
        if(rq != NULL) {
            free(rq);
        }
        return 0;
    }
    return -1;
}
/*
 * helper function
 */
// print the size of current queue
int queue_size(){
    int count= 1;
    struct request *temp = head;
    //struct request *further;
    while(NULL != (temp->tail)){
        temp = temp->tail;
        count = count +1;
    }
    return count;
}
void get_shortest_job(){
    bool head_is_shortest = true;
    struct request *parent = head; // previous node for shortest we find
    struct request *shortest = head; // shortest node
    struct request *tracker = head; // iterator
//    if(head->tail != NULL){
//        if((tracker->tail->content_size) < (shortest->content_size)){
//            head_is_shortest = false;
//            parent = tracker;
//            shortest = tracker->tail;
//        }
//    }
    while(tracker->tail!=NULL){
        if((tracker->tail->content_size) < (shortest->content_size)){
            head_is_shortest = false;
            parent = tracker;
            shortest = tracker->tail;
        }
        tracker = tracker->tail;
    }
    time_t now;
    time(&now);
    struct tm *Current = localtime(&now);
    char current_ts[250];
    //printf("\ncontent size:\"%d\"", size);
    strftime(current_ts, 250, "[%d/%b/%Y %H:%M:%S]", Current);

    if(!head_is_shortest){
        parent->tail = shortest->tail;
    }else{
        head = head ->tail;
    }
    ready_rq = shortest;
    strcpy(ready_rq->last_modified, current_ts);
}
void send_err_feedback(){
    if(errhead == NULL){}//nothing
    else {
        while (NULL != errhead->tail) {
            if (errhead != NULL) {
                write(sock, "\n", 1);
                write(sock, "Hello world muilti-thread server\n", 33);
                write(sock, "HTTP/1.1 400 ERROR\n", 19);
                write(sock, "Last Modified: ", 15);
                write(sock, errhead->last_modified, strlen(errhead->last_modified));
                write(sock, "\nERROR: ", 8);
                write(sock, errhead->msg, strlen(errhead->msg));
                write(sock, "\n", 1);
                write(sock, "Content: ", 9);
                write(sock, errhead->content, strlen(errhead->content));
                write(sock, "\n", 1);
            }
            errhead = errhead->tail;
        }
    }
}
void queue_err_feedback(struct invalid_request *rq){
    if(errhead == NULL ){
        errhead = rq;
    }else{
        struct invalid_request *temp = errhead;
        while(NULL!=temp->tail){
            temp = temp->tail;
        }
        temp->tail = rq;
    }

}
void file_log(char *info){
    if(log_file!=NULL){
        FILE *out = fopen(log_file,"a");
        fprintf(out,"%s",info);
        fprintf(out,"\n");
        fclose(out);
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