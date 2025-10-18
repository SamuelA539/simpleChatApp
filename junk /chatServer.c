#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>
#include <poll.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "chatServer.h"
#include "ServerClient.h"



//global vars

//TODO group into client struct
    // int connections[MAX_CONNECTIONS]; //cli fds
    // char *clientnames[MAX_CONNECTIONS]; //cli IPs/usernames

struct pollfd clients[MAX_CONNECTIONS];
mtx_t numClients_mtx, listening_mtx;

char *servport = "8080";
int listening, numClients = 0;

char *servMsg;
int servMessage = 0;
// int *currCon = connections; //last connected cli init to freespace??


//+cli args & ui
int main(int argc, char *argv[])  
{   
//cmd line args: port, maxNumClients ?backlog size
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
    
    printf("--- Welcome to Sam's Simple Chat App[server] ---\n"); //+set port & #clients.
    printf("Currently Using Port: %s\n\n", servport);  
    printf("\n---Menu options---\n\n"); //possible additions: listclients, change port, msg clients, msg client
    printMenu();
    
    mtx_init(&listening_mtx, mtx_plain);
    mtx_init(&numClients_mtx, mtx_plain);
    int sockfd, res;
    thrd_t listeningthread;
    
//serv UI
    int running = 1;
    char c;
    while(running) { //stall w/ input
        scanf(" %c", &c);
        switch(c)
        {
            case('h'):
                fflush(stdout);
                printf("\n---help menu---\n\n");
                printMenu();
                break;
            case('n'): //+client ips?
                fflush(stdout);
                mtx_lock(&numClients_mtx);
                printf("%d clients connected\n", (numClients + 1));
                mtx_unlock(&numClients_mtx);
                break;
            case('s'): //blocked @ accept so +1 cli after stop? && reset clientFD array?
                mtx_lock(&listening_mtx);
                if (listening != 0) {
                    listening = 0; //listening_mtx
                    thrd_join(listeningthread, &res);
                    close(sockfd);
                    printf("server listening stoped\n");    
                } else printf("Listening not listening\n");
                mtx_unlock(&listening_mtx);
                break;
            case('l'): 
                if (startListening(&listeningthread, &sockfd) != 0) {
                    fprintf(stderr, "Error Starting listening thread\n");
                    return 1;
                } 
                printf("listening on port: %s\n\n", servport);  
                break;
            case('p'):
                mtx_lock(&listening_mtx);
                if (listening) {
                    printf("Server is curently listening on port %s", servport);
                    printf("please stop listening before changing port");
                } else {
                    getPort(servport);
                    printf("enter l to start listening on port %s", servport);
                }
                mtx_unlock(&listening_mtx);
                break;
            case('q'):
                printf("quiting program\n");                              
                // closeServer(sockfd, connections, numCli);
                return 0;
            default:
                printf("input: %c not recognized\n", c);
                break;
        }
    }

    //clean up
    thrd_join(listeningthread, &res);
    mtx_destroy(&numClients_mtx);
    mtx_destroy(&listening_mtx);
    close(sockfd);
    return 0;   
}

//possible additions: listclients, msg clients, msg client
void printMenu() 
{
    printf("h: help menu\n");
    printf("l: start listening\n");//listclients?struct w name?
    printf("s: stop listening\n");
    printf("n: connected clients list\n");
    printf("q: quit program\n");
    printf("\n\n");
}

/**
 * binds socket and starts listening in seperate thread
 * 
 * returns 
 *   0 success
 *  -1 failure
 */
int startListening(thrd_t *listeningthrd, int *sockfd)
{
    if (bindSocket(servport, sockfd) == -1) {
        fprintf(stderr, "server failed to bind\n");
        return -1;
    }

    if (listen(*sockfd, BACKLOG) == -1) {
        perror("server: startListening: listen");
        return -1;
    }
    
    thrd_create(listeningthrd, servlisten, sockfd);

    return 0;
}



// --- THREAD  MAINS  --- 

//listening thread main
int servlisten(void *arg)
{
    int clientfd, res, *port = arg;
    struct sockaddr_storage clientaddrs;
    socklen_t clientaddrsize;
    char cliaddrstr[INET6_ADDRSTRLEN];

    struct pollfd servPortPoll;
    servPortPoll.fd = *port;
    servPortPoll.events = POLLIN;
    
    printf("--- Listening for Connections [port: %s] ---\n\n", servport);
    listening = 1;
    thrd_t clithrd;
    thrd_create(&clithrd, handleClients, NULL);
    // thrd_detach(clithrd);

    //listening loop
    while(listening) {
        mtx_lock(&numClients_mtx); 
        if (numClients < MAX_CONNECTIONS-1) {
            mtx_unlock(&numClients_mtx);  
            res = poll(&servPortPoll, 1, 5*1000); //5 sec timeout

            if (res > 0) {//event occured
                if (servPortPoll.revents & POLLIN) {    //accept
                    if (listening == 0 || (clientfd = accept(*port, (struct sockaddr*)&clientaddrs, &clientaddrsize)) == -1) { 
                        if (clientfd == -1) perror("server: servListen: accept");
                        break;
                        //return -1; //ends thread?
                    } else {
                        struct pollfd clientPoll;
                        clientPoll.fd = clientfd;
                        clientPoll.events = POLLIN | POLLOUT;

                        mtx_lock(&numClients_mtx);  
                        clients[numClients] = clientPoll; //posible error
                        // clients[numClients].fd = clientfd;
                        // clients[numClients].events = POLLOUT;
                        numClients++;
                        mtx_unlock(&numClients_mtx); 

                        //printing connection info
                        inet_ntop(clientaddrs.ss_family, 
                         get_in_addr((struct sockaddr *)&clientaddrs), 
                         cliaddrstr, sizeof cliaddrstr);
                        // printf("--server got connection from:  %s--\n", cliaddrstr);
                        printf(">>New Client - File Descriptor: %d(int), %d(poll)--\n", clientfd, clientPoll.fd);

                        //client thread 
                        // thrd_t clithrd;
                        // thrd_create(&clithrd, handleClients, NULL);
                        // thrd_detach(clithrd);                
                    }
                }           
                else if (servPortPoll.revents & POLLHUP) printf("POLLHUP - Peer channel Closed\n");
                else if (servPortPoll.revents & POLLNVAL) printf("POLLNVAL - invalid request: File Descriptor not open\n"); 
            } else if (res < 0) perror("server: servlisten: poll");   
            // else printf("poll timed out\n");
        } else {  //TODO
            mtx_unlock(&numClients_mtx); 
            printf("Too many clients connected please manage\n");
            // listening = 0; //stops listening loop ?when a client leaves?
        }
    }

    thrd_join(clithrd, &res);
    return 0;
}

//cycle clientfds for event  || thread per client polling
int handleClients()
{
    int polRes, res, monitoring = 1;
    char cliMsg[MAX_MESSAGE_SIZE];
    while (monitoring) {
        polRes = poll(clients, numClients, 5*1000);
        printf(">>client poll result: %d \t num clients: %d\n", polRes, numClients);
        if (polRes > 0) {
            mtx_lock(&numClients_mtx); 
            for (int i=0; i<numClients; i++){
                if (clients[i].revents & POLLIN) {//data to read
                    printf("data recived from client\n");
                    // if ((res = reciveMsg(clients[i].fd, cliMsg)) == -1) {
                    //     printf("reciveMsg Error client %d\n", i);
                    // } else if (res == 1) {
                    //     //edit array
                    //     printf("client disconnected\n");
                    // } else printf("client %d> %s\n", i, cliMsg);
                } else if (clients[i].revents & POLLOUT) { //if server has message send, skip  
                    printf("client %d ready to send\n", i);
                    if (servMessage) {
                        // printf("sending a message from server\n");
                    } else continue;
                } 
                else if (clients[i].revents & POLLHUP) printf("POLLHUP - Peer channel Closed\n");
                else if (clients[i].revents & POLLNVAL) printf("POLLNVAL - invalid request: File Descriptor not open\n"); 
            } 
            mtx_unlock(&numClients_mtx); 
        } else if(polRes < 0) printf("server client poll error\n");
        // else printf("client poll timeout\n");
    }
    return 0;
}


//client thread main
    //TODO: possible to manage clients directly array of threads
    //try using epoll (add clients)
        //thrd for reading: checks epoll then echos messgae 
        //thrd for writing?: ECHO:waits on all fds to be ready  MSG CLIENT: poll client speciifacally?
int handleClient(void *arg) 
{
    int running = 1;
    int *client = arg;
    int bytesrec, maxSz = 256;
    char recbuf[maxSz];

    int res;
    if (res = (send(*client, "Welcome To Chat",18, 0)) == -1) {
        perror("server: handlecli: send");
    }
    printf("welcome message sent [size: %d]\n", res);

    while(running) {
        memset(recbuf, 0, maxSz); //empty buf

        mtx_lock(&numClients_mtx);
        res = reciveMsg(*client, recbuf);
        if (res == 1) { //end connection msg
            running = 0;
            numClients--; 
            printf("client disconnected\n");
        } else {
            if(res == 0) {
                printf("\nrecived: %s\n", recbuf);
                // sendAllClients(recbuf, connections, numCli);
            } else {
                printf("Server recive Error");
                mtx_unlock(&numClients_mtx);
                running = 0;
                break;
            }
        }
        
    }
    close(*client);
    return 1;
}