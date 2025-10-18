#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "ServerClient.h"

//TODOs
//evaluate use of out params vs return vals

//sets sockaddr in IP specific struct
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//gets port number from user [TODO check port specifics]
int getPort(char *portStr)
{
    char buff[6];
    int port = -1;
    while (port < 0 || port > 65536) {
        printf("Enter new portnumber: ");
        scanf(" %s", buff);
        port = atoi(buff);
    }
    portStr = buff;
}


//high level send buff to socket 
/** 
 * sends message string to file descriptor
 * 
 * parameters 
 *  sockfd (in) - destination file descriptor
 *  msg (in) - message to be sent
 * 
 * return values
 *  0: success, -1: failure
**/
int sendMessage(int sockfd, char* msg) 
{
    int bytes, size;
    for (size=0; *(msg+size) != '\0'; size++);

    if ((bytes = send(sockfd, msg, size, 0)) == -1) {
        perror("ServerClientLib: sendMessage: send");
        return -1;
    }

    return bytes;
}


//high level recive msg [handles disconnect]
/**
 * recives input from given file descriptor
 *  closes fd on peer initiated disconnections (DISCONNECT_STRING recived)
 * 
 * params
 *  sockfd(in) - file descriptor of reciving socket
 *  buf(out) - buffer for data recived must be less than MAX_MESSAGE_SIZE macro
 * 
 * return values
 *  -1: failure, 0: disconected from peer, postiive int: message read
 */
int reciveMsg(int sockfd, char *buf) 
{
    int res;
    ssize_t bytes;
    if ((bytes = recv(sockfd, buf, MAX_MESSAGE_SIZE, 0)) == -1) {  
        perror("ServerClient: recive: recv");
        return -1;
    }

    // printf("recived %ld bytes\n", bytes);

    if ((res = strcmp(DISCONNECT_STRING, buf)) == 0) { //peer initaiate disconnection
        printf("disconnected from peer\n");
        close(sockfd); //needed?
        return 0;
    } else {
        if (bytes == 0) {
            printf("0 bytes recived\n");
            return -1;
        }
        return bytes;
    }         
}




// -- Client Methods -- 

//high level function for connecting to listening socket
/**
 * connects to given hostname and port
 * 
 * parms
 *  clisock (out param) - fd for connected socket
 *  hostname (in param) - hostname for destination
 *  portnum (in param) - portnumber for destination
 * 
 * return values
 *  0: success, -1: failure
 */
int connectToSocket(int *clisock, char *hostname, char *portnum) 
{
    int val, sock;
    char addrStr[INET6_ADDRSTRLEN];

    struct addrinfo hints, *results;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; //works for ipv4 & v6
    hints.ai_socktype = SOCK_STREAM;

    if ((val = getaddrinfo(hostname, portnum, &hints, &results)) != 0) {    //error handling
        fprintf(stderr, "addrinfo error: %s\n", gai_strerror(val));
        return -1;
    }
    
    struct addrinfo *p;
    for (p = results; p != NULL; p=p->ai_next) {
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("ServerClientLib: connectToSocket: socket");
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen ) == -1) {
            perror("ServerClientLib: connectToSocket: connect");
            continue;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), addrStr, sizeof addrStr);
        printf("connecting to server %s\n", addrStr);

        freeaddrinfo(results);
        *clisock = sock;
        return 0;
    }

    freeaddrinfo(results);
    return -1;
}

//high level disconnect from listeing socket[disconnect procedure]
/** 
 * initiates client disconnection
 * 
 * params
 * sockfd(in) - destination socket fd
 * 
 * returns
 *  0 - success
 *  -1 - failure
 **/
int disconnectFromServer(int sockfd) 
{
    if (send(sockfd, DISCONNECT_STRING, strlen(DISCONNECT_STRING), 0) == -1) { 
        perror("ServerClientLib: disconnect: send");
        return -1;
    }

    close(sockfd);
    return 0;
}




// --- Server Methods --- 

//high level socket bind func
/**
 *  creates listening file descriptor for given port  
 * 
 * params
 *  port(in) - string representation of port to bind to
 *  sockfd(out) - integer varaible to hold file descriptor for binded port
 * 
 * success: 1, failure: -1 
 */
int bindSocket(char *port, int *sockfd) 
{
    struct addrinfo hints, *servsocks, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; //AF_UNSPEC - IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    hints.ai_flags = AI_PASSIVE;

    int val;
    if((val = getaddrinfo(NULL, port, &hints, &servsocks))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(val));
        return -1;
    } 

    for (p=servsocks; p!=NULL; p=p->ai_next) {

        if ((*sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("ServerClientLib: bindSocket: socket");
            continue;
        }

        if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("ServerClientLib: bindSocket: bind");
            continue;
        }

        freeaddrinfo(servsocks);
        return 1;
    }

    freeaddrinfo(servsocks);
    return -1;
}

//TODO TEST
/**
 * sends all clients message string
 * 
 * params
 *  msg(in) - string to be sent to clients
 *  clientFD(in) - address for array of connect client file descriptors
 *  clientFDc(in) - number of client file descriptors in clientFD array
 * 
 * return values
 *  success: 1, failure: -1 
 */
int sendAllClients(char *msg, int *clientFD, int clientFDc)
{
    int res, diff=0;
    while(*(clientFD+diff) < clientFDc) {
        if ((res = send(*clientFD, msg, sizeof msg, 0)) == -1) {
            perror("ServerClientLib: sendAllClients: send"); //add client
            return -1;
        } 
        diff++;
    }
    return 1;
}

//high level discconect all listeners [disconnect procedure]
/**
 * notifies clients of shutdown and closes server's listening fd
 * 
 * params
 *  servfd(in) - file descriptor of servers listening port
 *  clientFD(in) - address for array of connect client file descriptors
 *  clientFDc(in) - number of client file descriptors in clientFD array
 * 
 * success: 1, failure: -1 
 */
int closeServer(int servfd, int *clientFD, int clientFDc) 
{
    int res;
    if ((res = sendAllClients(DISCONNECT_STRING, clientFD, clientFDc)) == -1) {
        perror("ServerClientLib: closeServer: sendAllClients");
        return -1;
    }

    close(servfd);
    return 1;
}