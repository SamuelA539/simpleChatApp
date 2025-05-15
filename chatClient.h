void *get_in_addr(struct sockaddr *sa);

int connectToSocket(struct addrinfo *adds);

int recive(void *arg);

void chat(int sock);