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

#include "chatClient.h"

#define MAXDATASIZE 257 //256 chars + '\0'
void chat(int sock);

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
int sockfd;
char* hostname = "127.0.0.1";
char* portnum = "8080";

//optional input args (serv IP(4 vs 6))
int main(int argc, int* argv[])
{
    if (argc == 2) {    //ip w/ default port?
        hostname = argv[1];
    }else if (argc == 3) {  //ip (v6 vs v4)  & port
        hostname = argv[1];
        portnum = argv[2];
    } else if (argc > 3 ){
        printf("usage: chatcli ip port || chatcli port ");
        return 1;
    }



    int val;
    if ((val = connectToSocket(&sockfd)) != 0){
        // perror("client: connectSocket");
        fprintf(stderr, "client failed to connect\n");
        return 2;
    }

    printf("---Connecting---\n\n");
        
    //recv thread
    thrd_t recvthrd;
    thrd_create(&recvthrd, recive, &sockfd);

    char msg[1024];
    int running = 1; 
//ui/sending thread
    while(running) {
        //menu: chat, close connection, change
        printf("Enter message to send: [quit -q, help -h]\n");
        scanf("%s", msg);

        switch(msg[0])
        {
            case '-':
                switch(msg[1])
                {
                    case 'q':
                        running = 0;
                        printf("quiting program\n");
                        msg[0] = EOF;
                        if (send(sockfd, msg,  strlen(msg), 0) == -1) perror("client: send");
                        return 0;
                        break;

                    case 'h':
                        printf("---help page---\n");
                        printf("action: comand\n");
                        printf("quit: -q\n");
                        printf("open chat: -c\n");
                        break;
                    
                    case 'c':
                        //full chat send and recive 
                        chat(sockfd);
                        break;
                    
                    default:
                        printf("please enter valid flag\n");
                        break;
                }
                break;

            //needed?
            case ' ':
                printf("please enter mesage or -q to quit\n");
                break;

            default: //add end char to msg
                if (send(sockfd, msg,  strlen(msg), 0) == -1) perror("client: send");
                break;
        }
    }

//clean up
    int res;
    thrd_join(recvthrd, &res);
    close(sockfd);
    return 0;
}


int connectToSocket(int *clisock) 
{
    int val, sock;
    char addrStr[INET6_ADDRSTRLEN];

    struct addrinfo hints, *results, *p;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; //works for ipv4 & v6
    hints.ai_socktype = SOCK_STREAM;

    if ((val = getaddrinfo(hostname, portnum, &hints, &results)) != 0) { //error handling
        fprintf(stderr, "addrinfo error: %s\n", gai_strerror(val));
        return -1;
    }
    

    for (p = results; p != NULL; p=p->ai_next) {

        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen ) == -1) {
            perror("client: connect");
            continue;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), addrStr, sizeof addrStr);
        printf("client: connecting to %s\n", addrStr);

        freeaddrinfo(results);
        *clisock = sock;
        return 0;
    }

    freeaddrinfo(results);
    return -1;
}

//thread to recv & display
int recive(void *arg) 
{
    int running = 1;
    int *sock = arg;
    
    int bytesrec, maxSz = 256;
    char recbuf[maxSz];
    
    while (running)
    {
        //handleclimsg(*client);   //recv messages 
        if ((bytesrec = recv(*sock, recbuf, maxSz-1, 0)) == -1) {  //handle error
            perror("server: recv");
            //error exit
        }
        
        if(recbuf[0] == EOF) {
            printf("server diconected\n");
            running = 0;
        } else {  //handle error
            //print test
            printf(">recived: %s\t[bytes: %d]\n", recbuf, bytesrec);
        }
        
    }
    return 1;
}

void chat(int sock) 
{
    //flushes output stream
    //fflush(stdout);

    printf("---chat page---\n");
    printf("-q to quit\n");

    //loop
        //thread reciving from serv and printing
        //thread scans ip and send o serv(should apper in chat)
    
    char recvbuf[1024];
    char msg[256];
    int bytesrec;
    int running = 1;
    
    while(running) 
    {        
        //recive 
        //!!!Fix
        if ((bytesrec = recv(sock, recvbuf, 1024-1, 0)) == -1) {
            perror("client: recv");
            //err exit
        }

        printf("> %s", recvbuf);

        //fork 
        if (!fork()) {  //scans & sends inputs       
            //printf("enter msg: ");
            scanf("enter msg: %s", msg);

            //send input
            if (send(sock, msg,  strlen(msg), 0) == -1) perror("client: send");
        }

    }

}