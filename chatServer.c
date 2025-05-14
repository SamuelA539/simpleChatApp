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

#define BACKLOG 10
#define MAXCONNECTIONS 5



void* get_in_addr(struct sockaddr *addr);
int getbindedsock(char *port); //binds to given sock address


//make return status nums

//threads
int servlisten(void *arg);
int handlecli(void *arg);

void handleclimsg(int clisock);
void sendallclis(char *msg);


//global vars
int connections[MAXCONNECTIONS];
char *clientnames[MAXCONNECTIONS];
int *currCon = connections;

char *servport = "8080"; //listening port


//+cli args & ui
int main(void)  //cmd line args: port num
{   
    int sockfd;

    //welcome & serv start conditions(port, set num clients)
    printf("--- welcome to Sam's simple chat app[server] ---\n\n"); //set port & #clients

    //if no cmd line args
    // printf("please enter port to run server on\n");
    // printf("please enter max number of clients to serve\n\n");


    if ((sockfd  = getbindedsock(servport)) == -1) {
        fprintf(stderr, "server failed to bind\n");
        return 2;
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("server: listen");
        return 1;
    }


    //thread
    thrd_t listengthread;
    thrd_create(&listengthread, servlisten, &sockfd);

    //listening
    printf("enter h for help\n");
    int running = 1;
    // char *str;
    char c;

//serv ui loop
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
                
                //send EOF
                char quitmsg[1] = {EOF};
                sendallclis(quitmsg);
                
                return 0;
            default:
                printf("input: %c not recognized\n", c);
                break;
        }
    }

    int res;
    thrd_join(listengthread, &res);

    printf("program done(main thrd)\n");
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
        if (currCon <= connections+MAXCONNECTIONS) { //only if enough spaces for connections

            //accepting connection
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
        } else printf("Too many clients connected please manage\n");
    }

    //join threads
    close(*port);
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
        } else {
            int res, *cur = connections;
            
            //echo to clis
            while(cur < currCon) {
                if (*cur == *client) { //sending client
                    printf("messageing messenger\n");
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



//???
void handleclimsg(int clisock) 
{
    int bytesrec, maxSz = 256;
    char recbuf[maxSz];

    if ((bytesrec = recv(clisock, recbuf, maxSz-1, 0)) == -1) {
        perror("server: recv");
        //error exit
    }
    
    //print test
    printf("\nrecived: %s\t[bytes: %d, client: ]\n", recbuf, bytesrec); //not needed for server

    if (recbuf[0] == EOF) {
        //cli disconect msg

    } else {
        //echo to clis
    }
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