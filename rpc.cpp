#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string>

#include <iostream>
#include <vector>
#include <string>

#include "rpc.h"

//temp defines
#define ADDRESS "stephen-Rev-1-0";
#define BPORT   50000
#define CPORT   3334

//perm defines
#define BACKLOG 5       //max # of queued connects
#define MAXHOSTNAME 100

//global var
int binder;
int binderfd, clientfd;
int port;
char server_address[MAXHOSTNAME + 1];

using namespace std;

//message types
enum message_type
{
    REGISTER,
    LOC_REQUEST,
    LOC_SUCCESS,
    LOC_FAILURE
};

//message struct
typedef struct
{
    char* port;
    string fn_name;
    string ip_address;
    //int* argTypes;
} message_sb;

int establish(unsigned short portnum)
{
    int sockfd, result;
    struct hostent *host;
    struct sockaddr_in my_addr;

    //get host info
    gethostname(server_address, MAXHOSTNAME);
    host = gethostbyname(server_address);
    if (host == NULL)
    {
        printf("gethost error\n");
        return -1;
    }

    //config socket
    memset(&my_addr, 0, sizeof(my_addr));   //clean memory
    my_addr.sin_family = host->h_addrtype;  //host address
    my_addr.sin_port = htons(portnum);      //input 0 to dynamically assign port
    my_addr.sin_addr.s_addr = INADDR_ANY;   //bind to local ip

    //get socket file descriptor
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("socket error: %i\n", sockfd);
        return sockfd;
    }

    //bind the socket
    result = bind(sockfd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (result < 0)
    {
        printf("bind error: %i\n", result);
        close(sockfd);
        return result;
    }

    //read the allocated port number
    int length = sizeof(my_addr);
    getsockname(sockfd, (struct sockaddr*)&my_addr, (socklen_t*)&length);
    port = ntohs(my_addr.sin_port);

    //print out the required env var
    printf("BINDER_ADDRESS %s\n", server_address);
    printf("BINDER_PORT %i\n", port);

    //listen for connections
    listen(sockfd, BACKLOG);

    //done
    return sockfd;
}

int get_connection(int sockfd)
{
    int newsockfd;

    //accept incoming calls in the given socket, return a new socket fd
    newsockfd = accept(sockfd, NULL, NULL);

    if (newsockfd < 0)
    {
        printf("accept error: %i\n", newsockfd);
        return newsockfd;
    }

    return (newsockfd);
}

int call_socket(char *hostname, int portnum)
{
    int sockfd, result;
    struct hostent *host;
    struct sockaddr_in my_addr;

    //get host info
    host = gethostbyname(hostname);
    if (host == NULL)
    {
        printf("gethost error\n");
        return -1;
    }

    //config socket
    memset(&my_addr, 0, sizeof(my_addr));   //clean memory
    memcpy((char *) &my_addr.sin_addr, host->h_addr, host->h_length);
    my_addr.sin_family =host->h_addrtype;
    my_addr.sin_port = htons(portnum);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    //get socket file descriptor
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("socket error: %i\n", sockfd);
        return sockfd;
    }

    //connect
    result = connect(sockfd, (struct sockaddr*)&my_addr, sizeof(struct sockaddr_in));
    if (result < 0)
    {
        printf("connect error: %i\n", result);
        return result;
    }

    //done
    return sockfd;
}

int rpcInit()
{
    printf("rpcInit\n");

    //create connection socket for client
    clientfd = establish(CPORT);
    if (clientfd < 0)
    {
        printf("establish error: %i\n", clientfd);
        return clientfd;
    }

	//create connection socket for client
    //	clientfd = establish(CPORT);
    //    if (clientfd < 0)
    //    {
    //        printf("establish error: %i\n", clientfd);
    //        return clientfd;
    //    }

    //open a connection to binder, for sending register request. keep this open

    //char* binder_address = getenv("BINDER_ADDRESS");
    //char* binder_port = getenv("BINDER_PORT");
    char* binder_address = (char*)ADDRESS;
    int binder_port=BPORT;

    //connect to the binder
    binderfd=call_socket(binder_address, binder_port);

    printf("rpcInit done\n");
    return 0;
}

int rpcCall(char* name, int* argTypes, void** args)
{
    printf("rpcCall\n");
    return 0;
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
    printf("rpcRegister\n");

    //rpcRegister("f0", argTypes0, *f0_Skel);

    //registers functions with binder
    //call binder, inform this function is available
    //0 returned for success registration

    //count argtypes length
    int arglen;
    for(arglen=0;;arglen++)
    {
        if(*(argTypes+arglen) == 0)
            break;
    }
    arglen++;

    int msglen = arglen*4 + strlen(name) + 1 + strlen(server_address) + 1 + 4;
    //int msglen = strlen(name) + 1 + strlen(server_address) + 1 + 4;
    int msgtype = REGISTER;

    //pack message
    message_sb msg;
    msg.port = serialize(port);
    msg.fn_name = name;
    msg.ip_address = server_address;
    //msg.argTypes = argTypes;

    cout << "function name: " << name << endl;

    send(binderfd, name, 3, 0);

    send(binderfd, (char*)&msglen, 4, 0);
    send(binderfd, (char*)&msgtype, 4, 0);
    send(binderfd, (char*)&msg, msglen, 0);

    printf("rpcRegister done\n");
    return 0;
}

char* serialize(int a)
{
    cout << "here1" << endl;
    char temp[4];
    temp[0] = a >> 24;
    temp[1] = a >> 16;
    temp[2] = a >> 8;
    temp[3] = a;
    cout << "here2" << endl;
    return temp;
}

int rpcExecute()
{
    printf("rpcExecute\n");
    return 0;
}

int rpcTerminate()
{
    printf("rpcTerminate\n");
    return 0;
}
