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
int *currCon = connections; //last connected cli init to freespace??
mtx_t currCon_mtx;

char *servport = "8080"; //listening port


//+cli args & ui
int main(int argc, char *argv[])  //cmd line args: port, maxNumClients ?backlog
{   
    // cmd line arg for port
    if (argc == 2) {
        int portInt = atoi(argv[1]);

        //printf("1:%s , 2:%s\n", argv[0], argv[1]);

        if (1023 < portInt && portInt < 49152 ) servport = argv[1];
        else {
            printf("%d is an Invalid Port:\tplease enter port number in ranage [1024, 49151]\n", portInt); 
            return 1;
        }
    } else if (argc > 2) {
        printf("usage: chatserv portNumber\n"); //port maxNumClients
        return 1;
    } 
    
    //welcome & serv start conditions(port, set num clients)
    printf("--- Welcome to Sam's Simple Chat App[server] ---\n"); //set port & #clients.
    printf("Using port: %s\n\n", servport);  
    mtx_init(&currCon_mtx, mtx_plain);
       
    int sockfd;
    thrd_t listengthread;

//starting listen thread (automatic)
    if (startListening(&listengthread, &sockfd) != 0) {
        fprintf(stderr, "Error Starging listening thread");
        return 1;
    }
    
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
                printf("n: number of connected clients\n");
                printf("q: quit program\n");
                //nested input
                break;
            case('n'):
                fflush(stdout);

                // mtx_lock(&currCon_mtx);
                printf("%ld Clients Currently Connected\n", (currCon - connections));
                // mtx_unlock(&currCon_mtx);
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

    //clean up
    int res;
    thrd_join(listengthread, &res);
    mtx_destroy(&currCon_mtx);
    close(sockfd);
    return 0;   
}



// --- General Functions --- 

void* get_in_addr(struct sockaddr *addr) 
{
    if (addr->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)addr)->sin_addr); 
    }
    return &(((struct sockaddr_in6*)addr)->sin6_addr);
}

//TODO: error handling msgs
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

        if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }

        if (bind(sock, p->ai_addr, p->ai_addrlen) == -1) {
            perror("server: bind");
            continue;
        }

        freeaddrinfo(servsocks);
        return sock;
    }

    freeaddrinfo(servsocks);
    return -1;
}

//TODO error handling msgs
int startListening(thrd_t *listeningthrd, int *sockfd) //0 on success
{
        
    if ((*sockfd  = getbindedsock(servport)) == -1) {
        fprintf(stderr, "server failed to bind\n");
        return 1;
    }

    if (listen(*sockfd, BACKLOG) == -1) { //proper error msg?
        perror("server: listen");
        return 1;
    }
    
    thrd_create(listeningthrd, servlisten, sockfd);

    return 0;
}

//TODO error handling msgs
void sendallclis(char *msg) 
{
    int res, *cur = connections;

    while(cur < currCon) {

        if ((res = send(*cur, msg, sizeof msg, 0)) == -1) {
            perror("server: send"); //add client
        } 

        cur++;
    }
}


//Test connection end strings
void closeServer() 
{
    char quitmsg[2] = {EOF, '\0'};
    sendallclis(quitmsg);
}

//send msg?


// --- THREAD  MAINS  --- 

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

        mtx_lock(&currCon_mtx);     //printf("Listening THRD: mtx locked(if)\n");
        if (currCon <= connections+MAXCONNECTIONS) {
            mtx_unlock(&currCon_mtx);      //printf("Listening THRD: mtx unlocked(if)\n");


            if ((clientfd = accept(*port, (struct sockaddr*)&clientaddrs, &clientaddrsize)) == -1) { //Error handle
                perror("server: accept");
                //return -1; //ends thread?
            } else {
                mtx_lock(&currCon_mtx);     //printf("Listening THRD: mtx locked\n");  
                *currCon = clientfd;
                currCon++;
                mtx_unlock(&currCon_mtx);   //printf("Listening THRD: mtx unlocked(currCon++)\n");

                //printing connection info
                inet_ntop(clientaddrs.ss_family, 
                    get_in_addr((struct sockaddr *)&clientaddrs), 
                    cliaddrstr, sizeof cliaddrstr);
                printf("--server got connection from:  %s--\n", cliaddrstr);

                //thread stuff
                thrd_t clithrd;
                thrd_create(&clithrd, handlecli, &clientfd);
                thrd_detach(clithrd);                
            }
        } else {  
            printf("Too many clients connected please manage\n");
            break; //stops listening loop ?when a client leaves?
        }
    }

    return 0;
}

//client thread main
    //TODO: possible to manage clients directly array of threads
int handlecli(void *arg) 
{
    int running = 1;

    int *client = arg;

    int bytesrec, maxSz = 256;
    char recbuf[maxSz];

    while(running) {
        memset(recbuf,0, maxSz); //empty buf

        if ((bytesrec = recv(*client, recbuf, maxSz-1, 0)) == -1) {
            perror("server: recv");
            //return -1; //error exit
        }
        
        printf("\nrecived: %s\t[bytes: %d, client: ]\n", recbuf, bytesrec);

        mtx_lock(&currCon_mtx);     //printf("Client THRD: mtx locked\n");
    
        if (recbuf[0] == EOF) { //end connection msg
            running = 0;
            currCon--; // race cond?
        } else sendallclis(recbuf);

        mtx_unlock(&currCon_mtx);   //printf("Client THRD: mtx unlocked\n");
    }

    close(*client);
    printf("client disconnected");
    return 1;
}