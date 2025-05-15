void* get_in_addr(struct sockaddr *addr);
int getbindedsock(char *port); //binds to given sock address

//threads
int servlisten(void *arg);
int handlecli(void *arg);

void handleclimsg(int clisock);
void sendallclis(char *msg);
