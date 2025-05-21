void *get_in_addr(struct sockaddr *sa);

//0 on success
int connectToSocket(int *clisock) ;

int recive(void *arg);

void chat(int sock);