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

#include "ServerClient.h"

int reciveThrd(void *arg);
int chat();



//global vars
int servfd, reciving; 
char* servHostname = "127.0.0.1";
char* servPort = "8080";

thrd_t reciverThrd;
mtx_t output_mtx, recive_mtx;



int main() 
{
   int res, running = 1;
   char c;

    mtx_init(&output_mtx, mtx_plain); //right to output
    mtx_init(&recive_mtx, mtx_plain); //recving thrd running

    
    while(running) { //program main loop
        sleep(1);

        //right to output [char scan]
        mtx_lock(&output_mtx);  //hold right to output
            printf("enter char: ");
            scanf(" %c", &c);
        mtx_unlock(&output_mtx);  //give up right to output
        

        if (c == 'q') { //quiting [program end]
            printf("quiting\n");
            running = 0;

            mtx_lock(&recive_mtx); //printf("*recive mtx locked - quit\n");  //locking recv mtx
                reciving = 0;
            mtx_unlock(&recive_mtx); //printf("*recive mtx unlocked - quit\n"); //unlocking recv mtx
            break;
        }
        
        if (c == 'c') { //connect [starts listening and reciving]
            if ((res = connectToSocket(&servfd, servHostname, servPort)) != 0) {
                printf("client connectToSocket error\n");
                exit(1);
            }

            chat(); //start chat
        }
    }


   //cleanup
   printf("cleaning threads\n"); 
   mtx_destroy(&output_mtx);
   thrd_join(reciverThrd, &res);
   close(servfd);
   printf("client program end\n"); 

   return 0;
}

//connects to chat
int chat() 
{
    int res, chatting = 1;
    char msgStr[MAX_MESSAGE_SIZE];
    
    struct pollfd servOutpoll;
    servOutpoll.fd = servfd;
    servOutpoll.events = POLLOUT;

    printf("---\tChating\t---\n\n");

    //wait for server connection noti ?timeout?
    if ((res = reciveMsg(servfd, msgStr)) < 0) {   //recive error
        perror("client: reciveThrd: reciveMsg");
        return 1;
    }
    if (strcmp(msgStr, CONNECT_STRING) != 0) {
        printf("Error establishing connection with server\n");
        printf("returning to menu\n");
        return 1;
    }
   
    reciving = 1;
    thrd_create(&reciverThrd, reciveThrd, &servfd);  //start recving thrd 

    mtx_lock(&recive_mtx);
    while (chatting && reciving) {
        mtx_unlock(&recive_mtx);
        res = poll(&servOutpoll, 1, 1000);      //output to socket poll
        mtx_lock(&output_mtx);   //printf("*output mtx locked - chat\n");   //right to output
        

        if (res > 0) { //poll fn success
            if (servOutpoll.revents & POLLOUT) { //pollout event
                printf("msg: ");
                scanf(" %s", msgStr);

                //make cooler disconnect  [signal for keybord?]
                if (strcmp(msgStr, "exit") == 0) { //client usr starts disconnect
                    chatting = 0; //kill chatting loop

                    //shuting off reciving loop[in thread?]?
                    mtx_lock(&recive_mtx); //printf("*recive mtx locked - chating exit\n");
                        reciving = 0;
                    mtx_unlock(&recive_mtx); //printf("*recive mtx unlocked - chating exit\n");

                    if (disconnectFromServer(servOutpoll.fd) == -1) perror("client: chat: disconnectFromServer"); //disconnect procedure?
                    else printf("* Disconnected from chat *\n\n");  
                    break;
                } else { //send msg
                    if (sendMessage(servOutpoll.fd, msgStr) == -1) {
                        perror("client: chat: sendMessage"); 
                        return 1;
                    }
                }
            } else { //poll errors
                printf("*client out poll error\n");
                if (servOutpoll.revents & POLLHUP) 
                    printf("!> POLLHUP - Peer channel Closed\n");
                
                if (servOutpoll.revents & POLLNVAL) 
                    printf("!> POLLNVAL - invalid request: File Descriptor not open\n"); 
                
                if (servOutpoll.revents & POLLERR) 
                    printf("!> POLLERR - error with file descriptor");
                
                break;
            }
        } else if (res < 0) { //poll error
            mtx_unlock(&output_mtx); 
            perror("client: chat: poll");
            return 1;
        }
    

        mtx_unlock(&output_mtx); //printf("*output mtx unlocked - chat\n");     //[end of loop?]give up right to output
        sleep(1);
        mtx_lock(&recive_mtx);
    }
    mtx_unlock(&recive_mtx);

    mtx_unlock(&output_mtx); // printf("chat end\n");                   //[final catch]give up right to input
    thrd_join(reciverThrd, &res); // printf("recive thrd ended\n");     //wait for recving thrd end
    
    return 0;
}


//thread for polling servfd to recived messages 
int reciveThrd(void *arg) 
{
    // printf("reciving thread started\n");

    char buff[MAX_MESSAGE_SIZE];
    memset(buff, '\0', MAX_MESSAGE_SIZE);

    int res, *servfd  = arg;

    struct pollfd servInpoll;
    servInpoll.fd = *servfd;
    servInpoll.events = POLLIN;
    
    mtx_lock(&recive_mtx); //printf("*recive mtx locked - recivethrd\n"); //[loop prep] recivng thrd mtx lock
    while(reciving) {
    mtx_unlock(&recive_mtx); //printf("*recive mtx unlocked - recivethrd\n");

        res = poll(&servInpoll, 1, 100);

        mtx_lock(&output_mtx);   //printf("*output mtx locked - reciving\n");       //right to output
            
        if (res > 0) { //poll success
            if (servInpoll.revents & POLLIN) {  //POLLIN event
                
                if ((res = reciveMsg(servInpoll.fd, buff)) < 0) {   //recive error
                    perror("client: recivieThrd: reciveMsg");
                    thrd_exit(1);
                } else if (res > 0) {                               //msg recived output
                    printf("# %s\t%d bytes\n", buff, res);   
                    memset(buff, '\0', MAX_MESSAGE_SIZE);
                } else {                                            //server starts disconnect
                    printf("* server disconnected *\n\n");
                    mtx_unlock(&output_mtx);
                    thrd_exit(0);
                }

            } else {    //poll event error [+ check triggers]
                printf("*client in poll error\n");
                if (servInpoll.revents & POLLHUP) 
                    printf("!> POLLHUP - Peer channel Closed\n");
                
                if (servInpoll.revents & POLLNVAL) 
                    printf("!> POLLNVAL - invalid request: File Descriptor not open\n"); 
                
                if (servInpoll.revents & POLLERR) 
                    printf("!> POLLERR - error with file descriptor");

                break;
            }
        } else if (res < 0) {   //poll fn error
            perror("client chat poll error");
            thrd_exit(1);                                           // quiting thrd kill
        }

        mtx_unlock(&output_mtx); //printf("*output mtx unlocked - reciving\n");     // give up right to output

    
    mtx_lock(&recive_mtx); //printf("*recive mtx locked - recivethrd\n");       //[next iteration prep?] locking loop mtx
    }
    mtx_unlock(&recive_mtx); //printf("*recive mtx unlocked - recivethrd\n");   //[final catch] reciving thrd mtx unlocked

    // printf("end of reciveThrd\n");
    thrd_exit(0);
}
