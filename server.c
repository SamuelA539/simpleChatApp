#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <threads.h>
#include <poll.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "ServerClient.h"

int serverListen(void *arg);
int reciveThrd(void *args);
int sendThrd(void *arg);
int sendClients(char *msg);



//global vars
thrd_t listenerThrd;

char *servport = "8080";
int listeningfd;

mtx_t clients_mtx, recive_mtx, output_mtx, listening_mtx; 
Client clients [MAX_CONNECTIONS];
int reciving, listening, numClients = 0;



int main() 
{
    int res, running  = 1;
    char c, *msgBuff;
    reciving = 1;

//inits   
    mtx_init(&clients_mtx, mtx_plain); //client list mtx 
    mtx_init(&recive_mtx, mtx_plain); //reciving mtx
    mtx_init(&output_mtx, mtx_plain); //right to output
    mtx_init(&listening_mtx, mtx_plain); //listening mtx 

    msgBuff = (char *)malloc(MAX_MESSAGE_SIZE * sizeof(char));
    if (msgBuff == NULL) {
        printf("message buffer allocation errror\n");
        thrd_join(listenerThrd, &res);
        mtx_destroy(&clients_mtx);
        close(listeningfd);
        return 1;
    }

//start listening [bind & start listening]
    if (bindSocket(servport, &listeningfd) == -1) {
        fprintf(stderr, "server failed to bind\n");
        return -1;
    } 
    printf("listening fd %d\n", listeningfd);
    thrd_create(&listenerThrd, serverListen, &listeningfd); //start listening thrd
       
//running loop
    while (running) {   //make input optional

        mtx_lock(&output_mtx); //printf("output mtx locked - main char entry\n");
        printf("enter character for comand[c - continue | q - quit | m - server message | l - client list]:");
        scanf(" %c", &c);
        mtx_unlock(&output_mtx); //printf("output mtx unlocked - main char entry\n");


        if (c == 'q') { //quit program
            printf("quiting program\n");

            running = 0;
            
            thrd_t sendingThrd;
            thrd_create(&sendingThrd, sendThrd, DISCONNECT_STRING);
            thrd_join(sendingThrd, &res);

            mtx_lock(&recive_mtx);
                reciving = 0;
            mtx_unlock(&recive_mtx);

            mtx_lock(&listening_mtx);
                listening = 0;
            mtx_unlock(&listening_mtx);

            printf("reciving off\n");
            printf("server disconnected\n");
            break;
        }
        
        if (c == 'm') { //server message 
            mtx_lock(&output_mtx); 
                memset(msgBuff, '\0', MAX_MESSAGE_SIZE);
                printf("msg: ");
                scanf(" %s", msgBuff);
            mtx_unlock(&output_mtx); 

            sendClients(msgBuff);

            // thrd_t sendingThrd;
            // thrd_create(&sendingThrd, sendThrd, msgBuff);
            // thrd_detach(sendingThrd);
        }     
        
        if (c == 'l') { //list clients
            mtx_lock(&clients_mtx);
            printf("--- Clients List ---\n");
            printf("number for actions relating to coresponding client\n");
            for (int i=0; i<numClients; i++) printf("%d) %s\n", i, clients[i].name);
            mtx_unlock(&clients_mtx);
        }
        
        sleep(3); //avoids overtaking
    }


//cleanup
    printf("cleaning threads\n");
    thrd_join(listenerThrd, &res); //stops program?
    mtx_destroy(&clients_mtx);  mtx_destroy(&recive_mtx);   mtx_destroy(&output_mtx);
    close(listeningfd);
    printf("server program end\n");

    return 0;
}

//listening thrd main - listens for connections
int serverListen(void *arg) 
{
    int *port = arg;
    int res, clientCount = 0;
    int clientfd;
    listening = 1;

    struct pollfd listeningPoll;
    listeningPoll.fd = *port;
    listeningPoll.events = POLLIN;

    struct sockaddr_storage clientaddrs;
    socklen_t clientaddrsize;



    if (listen(*port, 5) == -1) { 
        close(*port);
        perror("serverListen: listen");
        thrd_exit(1);
    }

    mtx_lock(&listening_mtx); //lock listening loop
    while (listening) {
        mtx_unlock(&listening_mtx);

        res = poll(&listeningPoll, 1, 1000);

        mtx_lock(&output_mtx);
        if (res > 0) { //poll success
            if (listeningPoll.revents & POLLIN) { //pollin event
                mtx_lock(&clients_mtx);

                //**ISSUE**
                if ((clientfd = accept(*port, (struct sockaddr*)&clientaddrs, &clientaddrsize)) == -1) {
                    perror("server: servListen: accept");
                    printf("Error accepting connection [%s, %d]\n", strerror(errno), errno); 
                    printf("listeing fd - %d\n", listeningPoll.fd);
                
                    printf("Please Quit Program\n");    // stop running?
                    mtx_unlock(&clients_mtx);
                    mtx_unlock(&output_mtx);
                    thrd_exit(1);
                }

                char nameBuff[65]; 
                sprintf(nameBuff, "client %d", ++clientCount);
                clients[numClients].fd = clientfd;
                clients[numClients].name = nameBuff;
                
                //timestamp
                time_t now;
                struct tm *time_now;
                char buf[256];

                time(&now);
                time_now = gmtime(&now);
                strftime(buf, 256, "%D %T", time_now);

                printf("%s>connection from client - %s[fd: %d]\n", buf, clients[numClients].name, clients[numClients].fd);
                
                //notify client of connection 
                int r;
                if ((r = sendMessage(clientfd, CONNECT_STRING)) == -1 ) {
                    perror("server: servListen: sendMessage");
                    printf("Error establishing connection to client [%s, %d]\n", strerror(errno), errno); 
                    printf("client fd - %d\n", clientfd);
                
                    printf("Please Quit Program\n");    // stop running?
                    mtx_unlock(&clients_mtx);
                    mtx_unlock(&output_mtx);
                    thrd_exit(1);
                }

                //start client recive thrd
                int cliIndex = numClients;
                thrd_t clientReciverThrd;
                thrd_create(&clientReciverThrd, reciveThrd, &cliIndex); //pass client index
                thrd_detach(clientReciverThrd);

                numClients++;
                mtx_unlock(&clients_mtx); 
            } else { //poll error event 
                printf("*server listening poll event error *\n\n");
                if (listeningPoll.revents & POLLHUP) 
                    printf("!> POLLHUP - Peer channel Closed\n");
                
                if (listeningPoll.revents & POLLNVAL) 
                    printf("!> POLLNVAL - invalid request: File Descriptor not open\n"); 
                
                if (listeningPoll.revents & POLLERR) 
                    printf("!> POLLERR - error with file descriptor");
                
                mtx_unlock(&output_mtx);
                thrd_exit(1);
            }
        } else if (res < 0) { //poll error
            perror("server: serverListen: poll");

            mtx_unlock(&output_mtx);
            thrd_exit(1);
        }
        mtx_unlock(&output_mtx); 

        mtx_lock(&listening_mtx); 
    }
    mtx_unlock(&listening_mtx); 
    printf("listening loop ended\n");

    thrd_exit(0);
}

//detaching thrd for recive message from a client [thrd per client?]
    //broken!!
    //alt for thrd_exits
int reciveThrd(void *arg)
{
    char buff[MAX_MESSAGE_SIZE]; 
    int res, *clientIndex = arg;
    
    //modifing client array
    mtx_lock(&clients_mtx);
    char *userName = clients[*clientIndex].name;
    mtx_unlock(&clients_mtx);


    struct pollfd clientInpoll;   
    clientInpoll.fd = clients[*clientIndex].fd;
    clientInpoll.events = POLLIN;

    // reciving = 1;

    mtx_lock(&recive_mtx);
    while (reciving) {
        printf("reciving loop[clientIndex: %d, fd: %d]\n", *clientIndex, clients[*clientIndex].fd);
        mtx_unlock(&recive_mtx);
        res = poll(&clientInpoll, 1, 5*1000);

        mtx_lock(&output_mtx); 
        if (res > 0) {
            printf("poll event [client %d]\n", *clientIndex);
            if (clientInpoll.revents & POLLIN) { //pollin event
                if ((res = reciveMsg(clientInpoll.fd, buff)) == -1) { //recving err
                    perror("server: reciveThrd: reciveMsg\n");
                    mtx_unlock(&output_mtx);

                    thrd_exit(1);
                } else if (res == 0) { //client disconnect
                    printf("* client disonnected *\n\n");
                    mtx_unlock(&output_mtx);
                    
                    //adjusting client list
                    mtx_lock(&clients_mtx);
                    numClients--;
                    mtx_unlock(&clients_mtx);

                    thrd_exit(0);
                } else { //msg recived
                    printf("%s> %s\t%d bytes\n", userName, buff, res);
                    memset(buff, '\0', MAX_MESSAGE_SIZE);
                }
            } else { //polling error Msgs 
                printf("* server recive[client fd %d] poll event error *\n\n", clientInpoll.fd);
                if (clientInpoll.revents & POLLHUP) 
                    printf("!> POLLHUP - Peer channel Closed\n");
                
                if (clientInpoll.revents & POLLNVAL) 
                    printf("!> POLLNVAL - invalid request: File Descriptor not open\n"); 
                
                if (clientInpoll.revents & POLLERR) 
                    printf("!> POLLERR - error with file descriptor");
                
                mtx_unlock(&output_mtx);
                thrd_exit(1);
            }
        } else if (res < 0) { //poll error
            perror("server: reciveThrd: poll");
            mtx_unlock(&output_mtx);
            thrd_exit(1);
        } 
        mtx_unlock(&output_mtx); 
        
        mtx_lock(&recive_mtx);
    }
    mtx_unlock(&recive_mtx);
    
    thrd_exit(0);
}


//thread to be called when there is message to send
    //exits with #polling errors - TODO check if there are clients
int sendThrd(void *arg) {
    char *msg = arg;
    int res, returnVal = 0;
    struct pollfd clientOutpoll;
    clientOutpoll.events = POLLOUT;

    mtx_lock(&clients_mtx); //locking client array
    for (int i=0; i<numClients; i++) {

        clientOutpoll.fd = clients[i].fd;  //poll each for output
        res = poll(&clientOutpoll, 1, 1000);

        mtx_lock(&output_mtx);
        if (res > 0) {
            if (clientOutpoll.revents & POLLOUT) { //client outpoll event
                if ((res = sendMessage(clientOutpoll.fd, msg)) == -1) {  //sending msg
                    printf("client %d\t", i);
                    perror("server: sendThrd: sendMessage");
                    continue;
                } 
            } else { //poll error event
                printf("* server send poll error[client %d - %s] poll event error*\n\n", i, clients[i].name);
                if (clientOutpoll.revents & POLLHUP) 
                    printf("POLLHUP - Peer channel Closed\n");
                
                if (clientOutpoll.revents & POLLNVAL) 
                    printf("POLLNVAL - invalid request: File Descriptor not open\n"); 

                if (clientOutpoll.revents & POLLERR) 
                    printf("POLLERR - error with file descriptor\n");

                continue;
            }
        } else if (res < 0) { //poll error 
            printf("client %d\t", i);
            perror("server: sendThrd: poll");
            returnVal++; 
            continue;
        }
        mtx_unlock(&output_mtx); //printf("*output mtx unlocked - sendThrd [client %d]\n", i);

    }
    mtx_unlock(&clients_mtx); //unlock client array
    
    thrd_exit(returnVal);
}


//sends server msg to clients
    //int -1 all, skips client passed
int sendClients(char *msg) {
    int res, returnVal = 0;
    struct pollfd clientOutpoll;
    clientOutpoll.events = POLLOUT;

    printf("send clients method\n");
    
    mtx_lock(&clients_mtx);
    printf("#clients %d\n", numClients);
    for (int i=0; i<numClients; i++) {

        printf("polling client: %s\n", clients[i].name);

        clientOutpoll.fd = clients[i].fd;  //poll each for output
        res = poll(&clientOutpoll, 1, 1000);

        mtx_lock(&output_mtx); //printf("*output mtx locked - sendThrd [client %d]\n", i);
            if (res > 0) {
                if (clientOutpoll.revents & POLLOUT) { //poll outevent
                    if ((res = sendMessage(clientOutpoll.fd, msg)) == -1) {
                        printf("client %d\t", i);
                        perror("server: sendThrd: sendMessage");
                        continue;
                    } 
                    else printf("message sent: client %d\n", i);
                } else { //poll error event
                    printf("* server poll error[client %d - %s] poll event error*\n\n", i, clients[i].name);
                    if (clientOutpoll.revents & POLLHUP) 
                        printf("POLLHUP - Peer channel Closed\n");
                    
                    if (clientOutpoll.revents & POLLNVAL) 
                        printf("POLLNVAL - invalid request: File Descriptor not open\n"); 

                    if (clientOutpoll.revents & POLLERR) 
                        printf("POLLERR - error with file descriptor\n");

                    continue;
                }
            } else if (res < 0) {  //poll error
                printf("client %d\t", i);
                perror("server: sendThrd: poll");
                returnVal++; 
                continue;
            }
        mtx_unlock(&output_mtx); //printf("*output mtx unlocked - sendThrd [client %d]\n", i);

    }
    mtx_unlock(&clients_mtx);
    
    // printf("sendClient end\n");
    return returnVal;
}