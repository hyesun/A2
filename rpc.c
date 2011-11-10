#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "rpc.h"

#define ADDRESS "hyesun-ubuntu";
#define BPORT   5555
#define CPORT   6666
#define BACKLOG 5       //max # of queued connects
#define MAXHOSTNAME 100

int establish(unsigned short portnum)
{
    int sockfd, result;
    struct hostent *host;
    struct sockaddr_in my_addr;
    char server_address[MAXHOSTNAME + 1];

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
    getsockname(sockfd, (struct sockaddr*)&my_addr, &length);

    //print out the required env var
    printf("SERVER_ADDRESS %s\n", server_address);
    printf("SERVER_PORT %i\n", ntohs(my_addr.sin_port));

    //listen for connections
    listen(sockfd, BACKLOG);

    //done
    return (sockfd);
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
	int result=0;
	int clientfd, binderfd;

	printf("rpcInit\n");

	//create connection socket for client
	clientfd = establish(CPORT);
    if (clientfd < 0)
    {
        printf("establish error: %i\n", clientfd);
        return clientfd;
    }

    //open a connection to binder, for sending register request. keep this open

    //char* binder_address = getenv("BINDER_ADDRESS");
    //char* binder_port = getenv("BINDER_PORT");
    char* binder_address = ADDRESS;
    int binder_port=BPORT;

    printf("binder address is: %s\n", binder_address);
    printf("binder port is: %i\n", binder_port);

    //connect to the binder
    binderfd=call_socket(binder_address, binder_port);
    printf("socketfd is %i\n", binderfd);


    printf("rpcInit done\n");
	return result;
}

int rpcCall(char* name, int* argTypes, void** args)
{
	int result=0;
	
	printf("rpcCall\n");
	return result;
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
	int result=0;
	
	printf("rpcRegister\n");
	return result;
}

int rpcExecute()
{
	int result=0;
	
	printf("rpcExecute\n");
	return result;
}

int rpcTerminate()
{
	int result=0;
	
	printf("rpcTerminate\n");
	return result;
}
