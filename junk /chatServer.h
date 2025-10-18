void printMenu();
int startListening(thrd_t *listeningthrd, int *sockfd);

//threads
int servlisten(void *arg);
int handlecli(void *arg);
int handleClients();


// void closeServer();