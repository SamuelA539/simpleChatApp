void* get_in_addr(struct sockaddr *addr);
int getbindedsock(char *port); //binds to given sock address

//threads
int servlisten(void *arg);
int handlecli(void *arg);

void handleclimsg(int clisock);
void sendallclis(char *msg);

//gets socekt and startst listeing thread
    //0 on success, else error
int startListening(thrd_t *listeningthrd, int *sockfd);
void closeServer();