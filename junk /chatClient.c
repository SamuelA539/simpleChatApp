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

#include "chatClient.h"
#include "ServerClient.h"


int sockfd;
char* hostname = "127.0.0.1";
char* portnum = "8080";

int connected = 0;
int receiving = 0;
thrd_t recvthrd;

int main(int argc, char* argv[])    //TODO find way to test other ips & ?need connect to serv w/o port?
{
//CMD line args
    if (argc == 2) {    //ip w/ default port(8080)?
        hostname = argv[1];
    } else if (argc == 3) {  //ip (v6||v4) & port
        hostname = argv[1];
        portnum = argv[2];
    } else if (argc > 3 ){
        printf("usage: chatcli ip port || chatcli port ");
        return 1;
    }

    printf("--- Welcome To Simple Chat App(Client)  ---\n"); //menu: chat, close connection, changeServer 
    printf("\n--- Menu ---\n\n");
    printMenu();

    int val, running;
    char c;

//ui & sending thread 
    running = 1;  
    while(running) {   
        scanf(" %c", &c);
        switch(c)
        {           
            case 'c':   //full chat (send and recive)
                if (receiving == 0) {
                    if ((val = connectToSocket(&sockfd, hostname, portnum)) != 0){
                        perror("client: connectToSocket");
                        fprintf(stderr, "client failed to connect\n");
                        break; 
                    }
                    printf("---Connecting---\n");
                    connected = 1;
                    chat();
                } else printf("chat currently active\n");
                break;
            case 'h': //help menu
                printf("---help page---\n");
                printMenu();
                break;
            case 'q': //quit program
                printf("quiting program\n");
                //disconnect only if connected
                if (connected) {
                    if (disconnectFromServer(sockfd) == -1) perror("disconect error: ");
                }
                running = 0;
                break;
            default:
                printf("Input not recognized\n");
                break;                
        }
    }
    close(sockfd);
    return 0;
}

void printMenu() 
{
    printf("quit program: q\n");
    printf("chat: c\n");
    printf("\n\n");
}


//issue: instant poll into messageing
void chat() 
{  
    printf("type exit to quit chat\n\n");
    
    char msgBuf[MAX_MESSAGE_SIZE];
    struct pollfd clipoll;
    clipoll.fd = sockfd;
    clipoll.events = POLLIN | POLLOUT;
    
    // thrd_create(&recvthrd, recives, &sockfd);

    int pollRes, res, chatting = 1;
    while (chatting) {
        pollRes = poll(&clipoll, 1, 5*1000); //5 sec timeout

        if (pollRes > 0) {
            if (clipoll.revents & POLLIN) {
                printf("data to read\n");
                res = reciveMsg(clipoll.fd, msgBuf);
                if (res == 0) printf("> %s\n", msgBuf);
                else if (res == 1) {
                    connected = 0;
                    break;
                } else perror("client: reciveMsg:");
            } else if (clipoll.revents & POLLOUT) { 
                memset(msgBuf, 0, strlen(msgBuf)+1);
                // printf("avaliable to send\n");

                printf("msg: ");
                scanf("%s", msgBuf);
                // printf("\n");
        
                if (strcmp(msgBuf, "exit") == 0) {
                    chatting = 0;
                    if (disconnectFromServer(sockfd) == -1) perror("chat: disconnectFromServer: ");
                    else printf("Disconnected from chat successfully\n");  
                    break;
                } else {
                    if (sendMesage(sockfd, msgBuf) == -1) perror("chat: sendMessage:");
                }
            } 
            else if (clipoll.revents & POLLHUP) printf("POLLHUP - Peer channel Closed\n");
            else if (clipoll.revents & POLLNVAL) printf("POLLNVAL - invalid request: File Descriptor not open\n"); 
            else if (clipoll.revents & POLLERR) printf("POLLERR - poll error occured\n"); 

        } else if (pollRes < 0) perror("client: chat: poll:");
    } 

    // thrd_join(recvthrd, &res);     
}



//handles sockets send & reciving
    //sending - store backlog of messages?
    //reciving - store backlog of recevided messages
int socketHandling()
{

}

// -- reciving & display thread main  --
int recive(void *arg) 
{
    receiving = 1;
    int res, *sock = arg;
    
    int bytesrec;
    char recbuf[MAX_MESSAGE_SIZE];
    
    int count = 3;
    while (receiving)
    {
        res = reciveMsg(*sock, recbuf);
        if (res == 0) {//message recived
            //add to file for storage?
        } else { //connection closing
            receiving = 0;
            if (res == 1) { //server diconnection
                printf("server diconnect\n");
            } else if (res == -1) {
                printf("clientRecive Error\n");
            }
            break;
        } 
    }
    return 1;
}