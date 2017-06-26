# CSE_421_WebServer
Project Name:
Multi-threaded Web Server


Authors:
Haoyu Guo
Or Khiyaev
Justyn Thompson


Project Description:
This is the server code to implement a multi-threaded web server in C++ on a UNIX
based platform. The server handles incoming HTTP/1.0 HEAD and GET requests.


Installation:
1. Clone this repository, with the base file of the source tree as /CSE_421_WebServer,
onto a UNIX-based platform.
2. Use the command 'make' to compile the source code into a program titled 'myhttpd'.
3. Run the program by typing in './myhttpd' followed by any command line arguments
you would like to pass to the server.


Usage:
myhttpd speaks a simplified version of HTTP/1.0 (according to RFC1945). Once launched,
the server will create a socket that will listen on port 8080 (unless otherwise
specified) for a client connection. The client is responsible for creating a socket
and attempting to connect to the server's port. The server will then accept the
connection. Once connected, the server will be able to handle client requests. The
client will send requests in the format of:
* <command> <argument> <HTTP version>
* <optional arguments>
* <blank line>
as defined by RFC1945. This simplified version of HTTP/1.0 recognized by the server
will only recognize requests containing the commands GET and HEAD. Once a request has
been made, the server will place it into a queue for a certain period of time before
scheduling and executing the requests. Upon servicing the request, the server will
generate a response in the format of:
* <HTTP version> <status code> <status message>
* <additional information>
* <blank line>
* <content>
as defined by RFC1945. The server will then write the response back to the client and
terminate the connection.

The server can also be prompted to enter a debugging mode by enabling the -d flag
in the command line arguments. By default, the server does not do any logging, but
this can be enabled by providing the -l flag to the command line arguments.


Example Usage:
    server                                client
I:  ./myhttpd
O:  


Synopsis:
myhttpd [-d] [-h] [-l file] [-p port] [-r dir] [-t time] [-n threadnum] [-s sched]


Input Variables:
file:             This option must be input as a string. It is the name of the file
                  that the server will write the logging history to.

port:             This option must be input as an integer. It is the port number
                  that the server will listen on to accept client connections.

dir:              This option must be input as a string. It is the name of the directory
                  that is to be set as the root directory for the server.

time:             This option must be input as an integer. It is the amount of time
                  (in seconds) that the server will wait for incoming requests and store
                  them in a ready queue before assigning them to execution threads.

threadnum:        This option must be input as an integer greater than or equal to 2.
                  It is the number of threads that will be available to execute/serve
                  incoming requests.

sched:            This option must be input as a string. There are only two valid
                  input strings; 'FCFS' and 'SJF'. They are the names of the scheduling
                  policies supported by this server.


Flags:
-d:               Enter debugging mode. Only accepts one connection at a time and
                  enables logging to stdout. Without this option the server will
                  run as a daemon process in the background.

-h:               Print a usage summary with all options and exit.

-l file:          Log all requests to the given 'file'. Requests will be logged in a
                  slight variation of Apache's so called "common" format following
                  the template '%a %t1 %t2 "%r" %>s %b' all in a single line per
                  request.
                  %a:   The remote IP address.
                  %t1:  The time the request was received by the queuing thread (GMT).
                  %t2:  The time the request was assigned to an execution thread
                        by the scheduler (GMT).
                  %r:   The (quoted) first line of the request.
                  %>s:  The status of the request.
                  %b:   Size of the response in bytes.

-p port:          Listen on the given 'port'. The default port number is 8080.

-r dir:           Set the root directory for the http server to 'dir'.

-t time:          Set the queuing time to 'time' seconds. The default queuing time
                  is 60 seconds.

-n threadnum:     Set the number of threads waiting ready in the execution thread
                  pool to 'threadnum'. The default is 4 execution threads.

-s sched:         Set the scheduling policy. The two 'sched' policies are FCFS and
                  SJF. The default scheduling policy is FCFS.
                  FCFS: An acronym for first come first serve. The task that arrives
                  at the server first will be scheduled first.
                  SJF:  An acronym for shortest job first. This particular server
                  uses a preemptive shortest job first algorithm. The task that takes
                  the least amount of remaining time to completion will be scheduled
                  first.


Credits:
Haoyu Guo - Primary Programmer

Or Khiyaev - Testing and Analysis, Secondary Debugger

Justyn Thompson - Secondary Programmer, Primary Debugger
