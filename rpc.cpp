using namespace std;
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

//perm defines
#define BACKLOG 5       //max # of queued connects
#define MAXHOSTNAME 20  //"hyesun-ubuntu"
#define MAXFNNAME 2     //"f0"
#define s_char 1        //size of char in bytes
#define s_int 4         //size of int in bytes
#define SUCCESS  0
#define FAILURE -1

//temp defines
#define ADDRESS "hyesun-ubuntu"
#define BPORT   33338
#define SPORT   0

//message types
enum message_type
{
    REGISTER,
    LOC_REQUEST,
    LOC_SUCCESS,
    LOC_FAILURE,
    EXECUTE,
    EXECUTE_SUCCESS,
    EXECUTE_FAILURE,
    TERMINATE
};

//local database for server
typedef struct
{
    char fn_name[MAXFNNAME+1];
    skeleton fn_skel;
}dataentry;

//global variables
int binder;
int binderfd, clientfd;
int port;
char server_address[MAXHOSTNAME + 1];
vector<dataentry> database;

// helper functions
int calcArgTypesLen(int* argTypes)
{
    int argTypesLen = 0;

    //reads the pointer until it reads 0, indicating end of array
    while(*(argTypes+argTypesLen) != 0)
    {
        argTypesLen++;
    }
    argTypesLen++;

    //return the num of elements in the array
    return argTypesLen;
}

int sizeOfArgs(int argType)
{
    switch (argType)
    {
        case ARG_CHAR:
            return sizeof(char);
        case ARG_SHORT:
            return sizeof(short);
        case ARG_INT:
            return sizeof(int);
        case ARG_LONG:
            return sizeof(long);
        case ARG_DOUBLE:
            return sizeof(double);
        case ARG_FLOAT:
            return sizeof(float);
        default:
            return -1;
    }
}

int establish(unsigned short portnum, int binder)
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
        return FAILURE;
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
    if (binder)
    {
      printf("BINDER_ADDRESS %s\n", server_address);
      printf("BINDER_PORT %i\n", port);

    }

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
        return FAILURE;
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

// main functions

int rpcInit()
{
    printf("rpcInit\n");

    //create connection socket for client
    clientfd = establish(SPORT, 0);
    if (clientfd < 0)
    {
        printf("establish error: %i\n", clientfd);
        return clientfd;
    }

    //open a connection to binder, for sending register request. keep this open

    //char* binder_address = getenv("BINDER_ADDRESS");
    //char* binder_port = getenv("BINDER_PORT");
    char* binder_address = (char*)ADDRESS;
    int binder_port=BPORT;

    //connect to the binder
    binderfd=call_socket(binder_address, binder_port);
    if (binderfd < 0)
    {
        printf("call socket error: %i\n", binderfd);
        return binderfd;
    }

    printf("rpcInit done\n");
    return SUCCESS;
}

int rpcCall(char* name, int* argTypes, void** args)
{
    //declarations
    char fn_server_address[MAXHOSTNAME + 1];
    int fn_server_port;

    //BINDER PART------------------------------------------

    //connect to binder
    binderfd=call_socket((char*)ADDRESS, BPORT);
    if (binderfd < 0)
    {
        printf("call socket error: %i\n", binderfd);
        return binderfd;
    }

    //get message info ready
    int argTypesLen = calcArgTypesLen(argTypes);
    int msglen = MAXFNNAME+s_char + argTypesLen*s_int;
    int msgtype = LOC_REQUEST;
    int checksum = msglen + sizeof(msgtype) + sizeof(msglen);

    //first send msglen and msgtype
    checksum-=send(binderfd, &msglen, sizeof(msglen), 0);
    checksum-=send(binderfd, &msgtype, sizeof(msgtype), 0);

    //send the main message
    checksum-=send(binderfd, name, MAXFNNAME+s_char, 0);
    checksum-=send(binderfd, argTypes, argTypesLen*s_int, 0);

    //check that all bytes have been sent
    if (checksum != 0)
    {
        printf("ERROR in rpcCall()\n");
        return FAILURE;
    }

    //get binder's reply
    recv(binderfd, &msglen, sizeof(msglen), 0);
    recv(binderfd, &msgtype, sizeof(msgtype), 0);
    checksum = msglen;
    if (msgtype == LOC_SUCCESS)
    {
        printf("loc_success\n");
        checksum-=recv(binderfd, fn_server_address, sizeof(fn_server_address), 0);
        checksum-=recv(binderfd, &fn_server_port, sizeof(fn_server_port), 0);
    }
    else if(msgtype == LOC_FAILURE)
    {
        int reasonCode;
        checksum-=recv(binderfd, &reasonCode, sizeof(reasonCode), 0);
        cout << "LOC_FAILURE. reasoncode = " << reasonCode << endl;
        close(binderfd);
        return reasonCode;
    }
    else
    {
        printf("ERROR in rpcCall()\n");
        return FAILURE;
    }

    //check that all bytes have been read
    if (checksum != 0)
    {
        printf("ERROR in rpcCall()\n");
        return FAILURE;
    }

    //done with binder
    close(binderfd);

    //SERVER PART------------------------------------------

    //connect to server
    int serverfd=call_socket(fn_server_address, fn_server_port);
    if (serverfd < 0)
    {
        printf("call socket error: %i\n", serverfd);
        return serverfd;
    }

    //get message info ready
    //argTypesLen = calcArgTypesLen(argTypes); already know
    msglen = MAXFNNAME+s_char;
    msgtype = EXECUTE;
    checksum = sizeof(msglen) + sizeof(msgtype) + msglen;

    //first send msglen and msgtype
    checksum-=send(serverfd, &msglen, sizeof(msglen), 0);
    checksum-=send(serverfd, &msgtype, sizeof(msgtype), 0);

    //send the main message
    checksum-=send(serverfd, name, MAXFNNAME+s_char, 0);
    checksum-=send(serverfd, argTypes, argTypesLen*s_int, 0);

    //send the args!!!!!!!!! so hard :(
    //count how long args are.




    printf("rpcCall done\n");
    return SUCCESS;
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
    printf("rpcRegister\n");

    //count argTypes length
    int argTypesLen = calcArgTypesLen(argTypes);

    //calculate message length in bytes (DON'T do sizeof() for name and argTypes)
    int msglen = sizeof(server_address) + sizeof(port) + MAXFNNAME+s_char + argTypesLen*s_int;

    //type of message is REGISTER
    int msgtype = REGISTER;

    //for checking send success
    int checksum = msglen + sizeof(msglen) + sizeof(msgtype);

    //first two bytes are always length and type
    checksum-=send(binderfd, &msglen, sizeof(msglen), 0);
    checksum-=send(binderfd, &msgtype, sizeof(msgtype), 0);

    //send the four components of the message (DON'T do sizeof() for name and argTypes)
    checksum-=send(binderfd, server_address, sizeof(server_address), 0);
    checksum-=send(binderfd, &port, sizeof(port), 0);
    checksum-=send(binderfd, name, MAXFNNAME+s_char, 0);
    checksum-=send(binderfd, argTypes, argTypesLen*s_int, 0);

    //make sure everything was successful
    if (checksum != 0)
    {
        printf("ERROR in rpcRegister()\n");
        return FAILURE;
    }

    //get binder reply
    int reply=FAILURE;
    checksum=recv(binderfd, &reply, sizeof(reply), 0);
    if (checksum != sizeof(reply))
    {
        printf("ERROR in rpcRegister()\n");
        return FAILURE;
    }
    if(reply != SUCCESS)
    {
        printf("ERROR in rpcRegister()\n");
        return FAILURE;
    }

    //record this function in local database
    dataentry record;
    strncpy(record.fn_name, name, 3);
    record.fn_skel=f;
    database.push_back(record);

    //read back the record
    for(int i=0; i<database.size(); i++)
    {
        int (*pf)(int*, void**);
        pf = database[i].fn_skel;
        printf("database entry [%i]: %s, %p\n", i, database[i].fn_name, pf);
    }

    printf("rpcRegister done\n");
    return SUCCESS;
}

int rpcExecute()
{
    printf("rpcExecute\n");

    //declare
    unsigned int* argTypes;
    int checksum, msglen, msgtype, argTypesLenByte;
    char fn_name[MAXFNNAME+s_char];

    //wait for client to call my socket
    int newsockfd = get_connection(clientfd);

    //read length and type
    recv(newsockfd, &msglen, sizeof(msglen), 0);
    recv(newsockfd, &msgtype, sizeof(msgtype), 0);

    //prepare argTypes array
    argTypesLenByte = msglen-sizeof(fn_name);
    argTypes = (unsigned int*)malloc(argTypesLenByte);

    //read main message
    recv(newsockfd, fn_name, sizeof(fn_name), 0);
    recv(newsockfd, argTypes, sizeof(argTypesLenByte), 0);

    //read args. so hard :(

    return SUCCESS;
}

int rpcTerminate()
{
    printf("rpcTerminate\n");
    return SUCCESS;
}
