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


#include "chatServer.h"


#define BACKLOG 10
#define MAXCONNECTIONS 5


//global vars
int connections[MAXCONNECTIONS]; //cli fds
char *clientnames[MAXCONNECTIONS]; //cli IPs

//point at?
int *currCon = connections; //last connected cli??

char *servport = "8080"; //listening port


//+cli args & ui
int main(int argc, char *argv[])  //cmd line args: port maxNumClients
{   
    if (argc == 1) {
        char *servport = argv[0]; //listening port.
        //check port nums
    } else {
        printf("usage: ./chatserv port maxNumClients"); //port maxNumClients
    } 
    int sockfd;

    //welcome & serv start conditions(port, set num clients)
    printf("--- welcome to Sam's simple chat app[server] ---\n\n"); //set port & #clients.

    //turn On option?

    if ((sockfd  = getbindedsock(servport)) == -1) {
        fprintf(stderr, "server failed to bind\n");
        return 1;
    }

    if (listen(sockfd, BACKLOG) == -1) { //proper error msg?
        perror("server: listen");
        return 1;
    }

//lisetning thread
    thrd_t listengthread;
    thrd_create(&listengthread, servlisten, &sockfd);

    
//serv UI
    printf("enter h for help\n");
    int running = 1;
    char c;
    while(running) { 

        scanf(" %c", &c);
        switch(c)
        {
            case('h'):
                fflush(stdout);
                printf("\n---help menu---\n\n");
                printf("q: quit program\n");
                //nested input
                break;
            case('q'):
                printf("quiting program\n");               
                
                char quitmsg[2] = {EOF, '\0'};
                sendallclis(quitmsg);

                return 0;

            default:
                printf("input: %c not recognized\n", c);
                break;
        }
    }

    close(sockfd);    
    int res;
    thrd_join(listengthread, &res);

    return 0;   
}




int getbindedsock(char *port) 
{
    int val, sockfd;
    struct addrinfo hints, *servsocks, *p;

    memset(&hints,0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((val = getaddrinfo(NULL, port, &hints, &servsocks))) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(val));
        return -1;
    } 

    for (p=servsocks; p!=NULL; p=p->ai_next) {
        int sock;
        //socket
        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        //bind
        if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: bind");
            continue;
        }

        freeaddrinfo(servsocks);
        return sock;
    }

    //p check needed?
    freeaddrinfo(servsocks);
    return -1;
}

void* get_in_addr(struct sockaddr *addr) 
{
    if (addr->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)addr)->sin_addr); 
    }
    return &(((struct sockaddr_in6*)addr)->sin6_addr);
}

// --- THREADS MAINS  --- 

//listening thread main
int servlisten(void *arg)
{
    int listening = 1; //global?

    int *port = arg;
    
    int clientfd; 
    struct sockaddr_storage clientaddrs;
    socklen_t clientaddrsize;
    char cliaddrstr[INET6_ADDRSTRLEN];

    printf("---Listening for Connections [port: %s]---\n\n", servport);

    //listening loop
    while(listening) { 

        if (currCon <= connections+MAXCONNECTIONS) {

            if ((clientfd = accept(*port, (struct sockaddr*)&clientaddrs, &clientaddrsize)) == -1) {
                perror("server: accept");
                // return -1; do something else
            } else {
                *currCon = clientfd; //placing fd in cli-list

                //printing connection info
                inet_ntop(clientaddrs.ss_family, 
                    get_in_addr((struct sockaddr *)&clientaddrs), 
                    cliaddrstr, sizeof cliaddrstr);
                printf("--server got connection from:  %s--\n", cliaddrstr);

                //thread
                thrd_t clithrd;
                //client thread(detached) -- how to kill?
                thrd_create(&clithrd, handlecli, &clientfd);
                thrd_detach(clithrd);

                currCon++;
            }
        } else {
            printf("Too many clients connected please manage\n");
            break; //stops accepting ?when client leaves?
        }
    }

    return 0;
}

//client thread main
int handlecli(void *arg) 
{
    int running = 1; //global?

    int *client = arg;

    int bytesrec, maxSz = 256;
    char recbuf[maxSz];

    //thread
    while(running) {
        //handleclimsg(*client);   //recv messages 
        if ((bytesrec = recv(*client, recbuf, maxSz-1, 0)) == -1) {
            perror("server: recv");
            //error exit
        }
        
        //print test
        printf("\nrecived: %s\t[bytes: %d, client: ]\n", recbuf, bytesrec); //not needed for server

        if (recbuf[0] == EOF) { //end connection char
            running = 0;
            close(*client);
            //currCon--; // race cond?
        } else {
            int res, *cur = connections;
            
            //echo to clis
                //alt: 
            while(cur < currCon) {
                if (*cur == *client) { //sending client
                    printf("messaging messenger\n");
                    if ((res = send(*cur, recbuf, bytesrec, 0)) == -1) {
                        perror("server: send");
                    }                  
                } else {
                    printf("messageing other\n");
                    if ((res = send(*cur, recbuf, bytesrec, 0)) == -1) {
                        perror("server: send");
                    }
                }
                cur++;
            }
           //emmpty reset recv buf
          
        }

    }

    close(*client); //closeing client
    return 1;
}


void sendallclis(char *msg) 
{
     int res, *cur = connections;
            
    //echo to clis
    while(cur < currCon) {

        if ((res = send(*cur, msg, sizeof msg, 0)) == -1) {
            perror("server: send");
        } 

        cur++;
    }
}

//method for server shutdown process
int closeServer() 
{

}