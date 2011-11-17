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
#include <strings.h>
#include <pthread.h>
#include "rpc.h"

//perm defines
#define BACKLOG 5       //max # of queued connects
#define MAXHOSTNAME 30  //"hyesun-ubuntu"
#define MAXFNNAME 50    //"f0"
#define s_char 1        //size of char in bytes
#define s_int 4         //size of int in bytes
#define SUCCESS  0
#define FAILURE -1
#define NUMTHREADS 3

//temp defines
//#define ADDRESS "hyesun-ubuntu"
#define BPORT   0
#define SPORT   0

//create threads on stack
pthread_t threads[NUMTHREADS];
pthread_mutex_t mutexsum;

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
int die = 0;
int binder;
int binderfd, clientfd;
int port;
char server_address[MAXHOSTNAME + 1];
vector<dataentry> database;

//================================================================
//  HELPER FUNCTIONS
//================================================================

int lenOfArgTypes(int* argTypes)
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

int sizeOfType(int argType)
{
    switch (argType)
    {
        case 0: //this is if argType=0
            return 0;
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

int getArgType(int *argType)
{
    //read the second byte, which is the type
    return ((*argType) & 0x00FF0000) >> 16;
}

int getArgLen(int *argType)
{
    //read the lower two bytes, which is the arg array size
    int argSize =  (*argType) & 0x0000FFFF;

    //if it's 0, it means it's a scalar - so set it to size 1
    if (argSize == 0)
        argSize = 1;

    return argSize;
}

int sizeOfArgs(int* argTypes)
{
    //declare
    int argTypesLen, typeSize, typeLen, totalLen = 0;

    //see how many elements are in array
    argTypesLen = lenOfArgTypes(argTypes);

    //get size of each element type
    for(int i=0; i<argTypesLen; i++)
    {
        //get size of the type
        typeSize = sizeOfType(getArgType(argTypes+i));

        //read lower two bytes to get low long the arg array is
        typeLen = getArgLen(argTypes+i);

        //update total length
        totalLen = totalLen + typeSize*typeLen;
    }

    return totalLen;
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
    my_addr.sin_port = htons((u_short)portnum);

    //get socket file descriptor
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("socket error: %i\n", sockfd);
        return sockfd;
    }

    //connect
    result = connect(sockfd, (struct sockaddr*)&my_addr, sizeof my_addr);
    if (result < 0)
    {
        printf("connect error: %i\n", result);
        return result;
    }

    //done
    return sockfd;
}

//================================================================
//  MAIN FUNCTIONS
//================================================================

int rpcInit()   //called by server
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
    char* binder_address = getenv("BINDER_ADDRESS");
    int binder_port= atoi(getenv("BINDER_PORT"));

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

int rpcCall(char* name, int* argTypes, void** args) //called by client
{
    printf("rpcCall\n");

    //declarations
    char fn_server_address[MAXHOSTNAME + 1];
    int fn_server_port;

    //get binder info
    char* binder_address = getenv("BINDER_ADDRESS");
    int binder_port= atoi(getenv("BINDER_PORT"));

    //================================================================
    //SEND BINDER LOC_REQUEST
    //================================================================

    //connect to binder
    binderfd=call_socket(binder_address, binder_port);
    if (binderfd < 0)
    {
        printf("call socket error: %i\n", binderfd);
        return binderfd;
    }

    //get message info ready
    int argTypesLen = lenOfArgTypes(argTypes);
    int msglen = MAXFNNAME+s_char + argTypesLen*s_int;
    int msgtype = LOC_REQUEST;

    //first send msglen and msgtype
    send(binderfd, &msglen, sizeof(msglen), 0);
    send(binderfd, &msgtype, sizeof(msgtype), 0);

    //send the main message
    send(binderfd, name, MAXFNNAME+s_char, 0);
    send(binderfd, argTypes, argTypesLen*s_int, 0);

    //get binder's reply
    recv(binderfd, &msglen, sizeof(msglen), 0);
    recv(binderfd, &msgtype, sizeof(msgtype), 0);

    if (msgtype == LOC_SUCCESS)
    {
        recv(binderfd, fn_server_address, sizeof(fn_server_address), 0);
        recv(binderfd, &fn_server_port, sizeof(fn_server_port), 0);
    }
    else if(msgtype == LOC_FAILURE)
    {
        int reasonCode;
        recv(binderfd, &reasonCode, sizeof(reasonCode), 0);
        close(binderfd);
        return reasonCode;
    }
    else
    {
        printf("ERROR in rpcCall()\n");
        return FAILURE;
    }

    //done with binder
    close(binderfd);

    //================================================================
    //SEND SERVER EXECUTE
    //================================================================

    //connect to server
    int serverfd=call_socket(fn_server_address, fn_server_port);
    if (serverfd < 0)
    {
        printf("call socket error: %i\n", serverfd);
        return serverfd;
    }

    //get message info ready
    int argLenByte = sizeOfArgs(argTypes);    //count how long args is
    msglen = MAXFNNAME+s_char + argTypesLen*s_int + argLenByte;
    msgtype = EXECUTE;

    //first send msglen and msgtype
    send(serverfd, &msglen, sizeof(msglen), 0);
    send(serverfd, &msgtype, sizeof(msgtype), 0);

    //send the main message
    send(serverfd, name, MAXFNNAME+s_char, 0);
    send(serverfd, argTypes, argTypesLen*s_int, 0);

    //send the main message - args
    for(int i=0; i<argTypesLen-1; i++)  //for each argument
    {
        int argTypeSize = sizeOfType(getArgType(argTypes+i));    //in bytes
        int argLen = getArgLen(argTypes+i);              //arg array length
        send(serverfd, (void*)args[i], argTypeSize*argLen, 0);
    }

    char fn_name[MAXFNNAME+s_char];

    //================================================================
    //GET SERVER REPLY
    //================================================================

    recv(serverfd, &msglen, sizeof(msglen), 0);
    recv(serverfd, &msgtype, sizeof(msgtype), 0);

    if (msgtype == EXECUTE_SUCCESS)
    {
        recv(serverfd, fn_name, sizeof(fn_name), 0);
        recv(serverfd, argTypes, argTypesLen*s_int, 0);
        for(int i=0; i<argTypesLen-1; i++)  //for each argument
        {
            int argTypeSize = sizeOfType(getArgType(argTypes+i));    //in bytes
            int argLen = getArgLen(argTypes+i);              //arg array length
            recv(serverfd, args[i], argTypeSize*argLen, 0);
        }
    }
    else if(msgtype == EXECUTE_FAILURE)
    {
        int reasonCode;
        recv(serverfd, &reasonCode, sizeof(reasonCode), 0);
        close(serverfd);
        return reasonCode;
    }
    else //what happened??
    {
        printf("rpcCall error\n");
    }

    //done with server
    close(serverfd);

    printf("rpcCall done\n");
    return SUCCESS;
}

int rpcRegister(char* name, int* argTypes, skeleton f)  //server calls
{
    printf("rpcRegister\n");

    //================================================================
    //GET MESSAGE READY
    //================================================================

    //count argTypes length
    int argTypesLen = lenOfArgTypes(argTypes);

    //calculate message length in bytes (DON'T do sizeof() for name and argTypes)
    int msglen = sizeof(server_address) + sizeof(port) + MAXFNNAME+s_char + argTypesLen*s_int;

    //type of message is REGISTER
    int msgtype = REGISTER;

    //================================================================
    //SEND TO BINDER
    //================================================================

    //first two bytes are always length and type
    send(binderfd, &msglen, sizeof(msglen), 0);
    send(binderfd, &msgtype, sizeof(msgtype), 0);

    //send the four components of the message (DON'T do sizeof() for name and argTypes)
    send(binderfd, server_address, sizeof(server_address), 0);
    send(binderfd, &port, sizeof(port), 0);
    send(binderfd, name, MAXFNNAME+s_char, 0);
    send(binderfd, argTypes, argTypesLen*s_int, 0);

    //================================================================
    //GET BINDER REPLY
    //================================================================

    //get binder reply
    int reply=FAILURE;
    recv(binderfd, &reply, sizeof(reply), 0);

    if(reply != SUCCESS)
    {
        printf("ERROR in rpcRegister()\n");
        return FAILURE;
    }

    //================================================================
    //RECORD THE FUNCTION IN DATABASE
    //================================================================

    dataentry record;
    strncpy(record.fn_name, name, 3);
    record.fn_skel=f;
    database.push_back(record);

    printf("rpcRegister done\n");
    return SUCCESS;
}

void* getClientRequest(void* arg)
{
    while (1)
    {
        //declare
        int msglen, msgtype;
        char fn_name[MAXFNNAME + s_char];

        //================================================================
        //RECV FROM CLIENT
        //================================================================

        //wait for client to call my socket
        int newsockfd = get_connection(clientfd);

        //read length and type
        recv(newsockfd, &msglen, sizeof(msglen), 0);
        recv(newsockfd, &msgtype, sizeof(msgtype), 0);

        //prepare argTypes array
        int argsCumulativeSize = msglen - sizeof(fn_name);
        int *argsCumulative = (int*) malloc(argsCumulativeSize);

        //read main message
        recv(newsockfd, fn_name, sizeof(fn_name), 0);
        recv(newsockfd, argsCumulative, argsCumulativeSize, MSG_WAITALL);

        //================================================================
        //UNPACK ARGSCUMULATIVE
        //================================================================

        //first, see how many args there are
        int argTypesLen = lenOfArgTypes(argsCumulative);

        //figure out argTypes
        int *argTypes = (int*) malloc(argTypesLen * s_int);
        memcpy(argTypes, argsCumulative, argTypesLen * s_int);

        //figure out args
        int argsSize = argsCumulativeSize - argTypesLen * s_int; //in bytes
        void** args = (void**) malloc((argTypesLen - 1) * sizeof(void*));
        void* argsIndex = argsCumulative + argTypesLen; //point to the correct place

        for (int i = 0; i < argTypesLen - 1; i++)
        {
            //see what type/len of arg we're dealing with
            int arg_type = getArgType(argTypes + i);
            int arg_type_size = sizeOfType(arg_type);
            int arr_size = getArgLen(argTypes + i);

            //temp holder
            void* args_holder = (void*) malloc(arr_size * arg_type_size);

            //copy the address
            *(args + i) = args_holder;

            //copy the contents of array into temp holder
            for (int j = 0; j < arr_size; j++)
            {
                void* temp = (char*) args_holder + j * arg_type_size;
                memcpy(temp, argsIndex, arg_type_size);
                argsIndex = (void*) ((char*) argsIndex + arg_type_size);
            }
        }

        //================================================================
        //SEND TO SKELETON FUNCTION
        //================================================================

        int executionResult = FAILURE;
        for (int i = 0; i < database.size(); i++)
        {
            string a = database[i].fn_name;
            string b = fn_name;
            if (a == b) //look for the function name
            {
                executionResult = database[i].fn_skel(argTypes, args);
            }
        }

        //================================================================
        //SEND THE RESULTS BACK TO CLIENT
        //================================================================

        if (executionResult == SUCCESS)
        {
            msgtype = EXECUTE_SUCCESS;
            send(newsockfd, &msglen, sizeof(msglen), 0);
            send(newsockfd, &msgtype, sizeof(msgtype), 0);
            send(newsockfd, fn_name, sizeof(fn_name), 0);
            send(newsockfd, argTypes, argTypesLen * s_int, 0);
            for (int i = 0; i < argTypesLen - 1; i++) //for each argument
            {
                int argTypeSize = sizeOfType(getArgType(argTypes + i)); //in bytes
                int argLen = getArgLen(argTypes + i); //arg array length
                send(newsockfd, args[i], argTypeSize * argLen, 0);
            }
        }
        else if (executionResult == FAILURE)
        {
            //send error code back
            msgtype = EXECUTE_FAILURE;
            int reasonCode = executionResult;
            send(newsockfd, &msglen, sizeof(msglen), 0);
            send(newsockfd, &msgtype, sizeof(msgtype), 0);
            send(newsockfd, &reasonCode, sizeof(reasonCode), 0);
        }
        else //what happened??
        {
            printf("rpcExecute error\n");
        }
        if(die==1)
        {
            printf("terminating clientrequest thread\n");
            exit(0);
        }
    }
}

void* listenForTerminate(void *arg)
{
    while(1)
    {
        //listen for termination from binder
        int msglen, msgtype;
        recv(binderfd, &msglen, sizeof(msglen), 0);
        recv(binderfd, &msgtype, sizeof(msgtype), 0);

        if (msgtype == TERMINATE)
        {
            printf("terminate recieved\n");
            die = 1;
            exit(0);
        }

    }
}

int rpcExecute()
{
    printf("rpcExecute\n");

    //listenForTerminate
    if(pthread_create(&threads[0], NULL, listenForTerminate, NULL))
    {
        printf("ERROR creating thread %d", 1);
        exit(-1);
    }

    //getClientRequest
    if(pthread_create(&threads[1], NULL, getClientRequest, NULL))
    {
        printf("ERROR creating thread %d", 0);
        exit(-1);
    }

    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);

    printf("rpcExecute done\n");
    return SUCCESS;
}

int rpcTerminate()
{
    printf("rpcTerminate\n");

    //prepare message
    int msglen = 0;
    int msgtype = TERMINATE;

    //open a connection to binder, for sending register request. keep this open
    char* binder_address = getenv("BINDER_ADDRESS");
    int binder_port= atoi(getenv("BINDER_PORT"));

    //connect to the binder
    binderfd=call_socket(binder_address, binder_port);
    if (binderfd < 0)
    {
        printf("call socket error: %i\n", binderfd);
        return binderfd;
    }

    //send it to binder
    send(binderfd, &msglen, sizeof(msglen), MSG_WAITALL);
    send(binderfd, &msgtype, sizeof(msgtype), MSG_WAITALL);

    printf("rpcTerminate done\n");
    return SUCCESS;
}
