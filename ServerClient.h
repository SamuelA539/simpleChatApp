#define DISCONNECT_STRING "DISCONNECT"
#define CONNECT_STRING "CONNECTED"
#define MAX_MESSAGE_SIZE 513
#define BACKLOG 10
#define MAX_CONNECTIONS 5

//object describing peer?
typedef struct Client {
    int fd;
    char *name;
    // struct pollfd *poll;
    struct Client *nextClient;
} Client;



//sets sockaddr in IP specific struct
void *get_in_addr(struct sockaddr *sa);

//gets port number from user
int getPort(char *portStr);

/** 
 * sends message string to file descriptor
 * 
 * parameters 
 *  sockfd (in) - destination file descriptor
 *  msg (in) - message to be sent
 * 
 * return values
 *  0: success, -1: failure
**/
int sendMessage(int sockfd, char* msg);

/**
 * recives input from given file descriptor
 *  closes fd on peer initiated disconnections (DISCONNECT_STRING recived)
 * 
 * params
 *  sockfd(in) - file descriptor of reciving socket
 *  buf(out) - buffer for data recived must be less than MAX_MESSAGE_SIZE macro
 * 
 * return values
 *  -1: failure, 0: disconected from peer, postiive int: message read
 */
int reciveMsg(int sockfd, char *buf);

// -- Client Methods -- 

/**
 * connects to given hostname and port
 * 
 * parms
 *  clisock (out param) - fd for connected socket
 *  hostname (in param) - hostname for destination
 *  portnum (in param) - portnumber for destination
 * 
 * return values
 *  0: success, -1: failure
 */
int connectToSocket(int *clisock, char *hostname, char *portnum);

/** 
 * initiates client disconnection
 * 
 * params
 * sockfd(in) - destination socket fd
 * 
 * return values
 *  0: success, -1: failure
 **/
int disconnectFromServer(int sockfd);




// --- Server Methods --- 

/**
 *  creates file descriptor for given port  
 * 
 * params
 *  port(in) - string representation of port to bind to
 *  sockfd(out) - integer varaible to hold file descriptor for binded port
 * 
 * return values
 *  success: 1, failure: -1 
 */
int bindSocket(char *port, int *sockfd);

/**
 * sends all clients message string
 * 
 * params
 *  msg(in) - string to be sent to clients
 *  clientFD(in) - address for array of connect client file descriptors
 *  clientFDc(in) - number of client file descriptors in clientFD array
 * 
 * return values
 *  success: 1, failure: -1 
 */
int sendAllClients(char *msg, int *clientFD, int clientFDc);


/**
 * notifies clients of shutdown and closes server's listening fd
 * 
 * params
 *  servfd(in) - file descriptor of servers listening port
 *  clientFD(in) - address for array of connect client file descriptors
 *  clientFDc(in) - number of client file descriptors in clientFD array
 * 
 * return values
 *  success: 1, failure: -1 
 */
int closeServer(int servfd, int *clientFD, int clientFDc);