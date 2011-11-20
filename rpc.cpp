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
#include <stdexcept>
#include "rpc.h"

#define BACKLOG 50       //max # of queued connects
#define MAXHOSTNAME 50  //"hyesun-ubuntu"
#define MAXFNNAME 50    //"f0"
#define SUCCESS  0
#define FAILURE -1
#define BPORT   0
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
    TERMINATE,
    TERMINATE_SUCCESS
};

//error codes - negative
enum error_code
{
    ARG_ARRAY_TOO_LONG = -14,
    ARG_TYPES_MISMATCH,
    OUTPUT_INPUT_MISMATCH,
    ESTABLISH_ERROR,
    CALLSOCKET_ERROR,
    SERVER_NOT_FOUND,
    BINDER_NOT_FOUND,
    EXECUTION_ERROR,
    REGISTER_FAIL,
    SEND_ERROR,
    RECV_ERROR,
    THREAD_ERROR,
    SERVER_FN_NOT_FOUND
};

//warning codes - positive
enum warning_code
{
    DUPLICATE_FN=-2,
    TEST
};

//local database for server
typedef struct
{
    char fn_name[MAXFNNAME+1];
    skeleton fn_skel;
}dataentry;

//global variables
int die = 0;
int server_port;
int binderfd, clientfd;
char server_address[MAXHOSTNAME+1];

//server's database of its functions
vector<dataentry> database;

//thread info
int threadcount = 1;
pthread_mutex_t mutexsum;

//================================================================
//  HELPER FUNCTIONS
//================================================================

void send_safe(int sockfd, const void *msg, int len, int flags)
{
    if (send(sockfd, msg, len, flags) != len)
    {
        throw SEND_ERROR;
    }
}

void recv_safe(int sockfd, void *buf, int len, int flags)
{
    if (recv(sockfd, buf, len, flags) != len)
    {
        throw RECV_ERROR;
    }
}

void threadDec()
{
    pthread_mutex_lock (&mutexsum);
    threadcount--;
    pthread_mutex_unlock (&mutexsum);
}

void threadInc()
{
    pthread_mutex_lock (&mutexsum);
    threadcount++;
    pthread_mutex_unlock (&mutexsum);
}

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
    for(int i=0; i<argTypesLen-1; i++)
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
    int port, sockfd, result;
    struct hostent *host;
    struct sockaddr_in my_addr;
    char address[MAXHOSTNAME+1];

    //get host info
    gethostname(address, MAXHOSTNAME);
    host = gethostbyname(address);
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

    if (binder) //binder only - print out the required env var
    {
      printf("BINDER_ADDRESS %s\n", address);
      printf("BINDER_PORT %i\n", port);

    }
    else    //servers only - save these for use in rpcRegister
    {
        strcpy(server_address, address);
        server_port = port;
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
        return ESTABLISH_ERROR;

    //open a connection to binder, for sending register request. keep this open
    char* binder_address = getenv("BINDER_ADDRESS");
    int binder_port= atoi(getenv("BINDER_PORT"));
    if(binder_address==NULL || binder_port<=0)
        return BINDER_NOT_FOUND;

    //connect to the binder - this stays OPEN until the server terminates
    binderfd=call_socket(binder_address, binder_port);
    if (binderfd < 0)
        return CALLSOCKET_ERROR;

    printf("rpcInit done\n");
    return SUCCESS;
}

int rpcCall(char* name, int* argTypes, void** args) //called by client
{
    printf("rpcCall\n");

    //server info
    char server_address[MAXHOSTNAME+1];
    int server_port;

    //binder info
    char* binder_address = getenv("BINDER_ADDRESS");
    int binder_port= atoi(getenv("BINDER_PORT"));
    if(binder_address==NULL || binder_port<=0)
        return BINDER_NOT_FOUND;

    //================================================================
    //SEND BINDER LOC_REQUEST
    //================================================================

    //connect to binder
    binderfd=call_socket(binder_address, binder_port);
    if (binderfd < 0)
        return CALLSOCKET_ERROR;

    //get message info ready
    int argTypesLen = lenOfArgTypes(argTypes);
    int msglen = MAXFNNAME+sizeof(char) + argTypesLen*sizeof(int);
    int msgtype = LOC_REQUEST;

    //first send msglen and msgtype
    send(binderfd, &msglen, sizeof(msglen), 0);
    send(binderfd, &msgtype, sizeof(msgtype), 0);

    //send the main message
    send(binderfd, name, MAXFNNAME+sizeof(char), 0);
    send(binderfd, argTypes, argTypesLen*sizeof(int), 0);

    //get binder's reply
    recv(binderfd, &msglen, sizeof(msglen), 0);
    recv(binderfd, &msgtype, sizeof(msgtype), 0);

    if (msgtype == LOC_SUCCESS)
    {
        recv(binderfd, server_address, sizeof(server_address), 0);
        recv(binderfd, &server_port, sizeof(server_port), 0);
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
    int serverfd=call_socket(server_address, server_port);
    if (serverfd < 0)
        return CALLSOCKET_ERROR;

    //get message info ready
    int argLenByte = sizeOfArgs(argTypes);    //count how long args is
    msglen = MAXFNNAME+sizeof(char) + argTypesLen*sizeof(int) + argLenByte;
    msgtype = EXECUTE;

    //first send msglen and msgtype
    send(serverfd, &msglen, sizeof(msglen), 0);
    send(serverfd, &msgtype, sizeof(msgtype), 0);

    //send the main message
    send(serverfd, name, MAXFNNAME+sizeof(char), 0);
    send(serverfd, argTypes, argTypesLen*sizeof(int), 0);

    //send the main message - args
    for(int i=0; i<argTypesLen-1; i++)  //for each argument
    {
        int argTypeSize = sizeOfType(getArgType(argTypes+i));    //in bytes
        int argLen = getArgLen(argTypes+i);              //arg array length
        send(serverfd, (void*)args[i], argTypeSize*argLen, 0);
    }

    char fn_name[MAXFNNAME+1];

    //================================================================
    //GET SERVER REPLY
    //================================================================

    recv(serverfd, &msglen, sizeof(msglen), 0);
    recv(serverfd, &msgtype, sizeof(msgtype), 0);

    if (msgtype == EXECUTE_SUCCESS)
    {
        recv(serverfd, fn_name, sizeof(fn_name), 0);
        recv(serverfd, argTypes, argTypesLen*sizeof(int), 0);
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
    int msglen = sizeof(server_address) + sizeof(server_port) + MAXFNNAME+sizeof(char) + argTypesLen*sizeof(int);

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
    send(binderfd, &server_port, sizeof(server_port), 0);
    send(binderfd, name, MAXFNNAME+sizeof(char), 0);
    send(binderfd, argTypes, argTypesLen*sizeof(int), 0);

    //================================================================
    //GET BINDER REPLY
    //================================================================

    //get binder reply
    int reply=FAILURE;
    recv(binderfd, &reply, sizeof(reply), 0);

    if(reply != SUCCESS)
        return REGISTER_FAIL;

    //================================================================
    //RECORD THE FUNCTION IN DATABASE
    //================================================================

    dataentry record;
    strncpy(record.fn_name, name, MAXFNNAME+1);
    record.fn_skel=f;
    database.push_back(record);

    printf("rpcRegister done\n");
    return SUCCESS;
}

void* serveClientRequest(void* newsockfdptr)
{
    //declare
    int msglen, msgtype;
    char fn_name[MAXFNNAME+1];
    int newsockfd = *(int*)newsockfdptr;

    //================================================================
    //RECV FROM CLIENT
    //================================================================

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
    int *argTypes = (int*) malloc(argTypesLen * sizeof(int));
    memcpy(argTypes, argsCumulative, argTypesLen * sizeof(int));

    //figure out args
    int argsSize = argsCumulativeSize - argTypesLen * sizeof(int); //in bytes
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
        send(newsockfd, argTypes, argTypesLen * sizeof(int), 0);
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

    //bye client
    close(newsockfd);

    //clean up
    for(int i=0; i<argTypesLen-1; i++)
    {
        free(*(args+i));
    }
    free(args);
    free(argTypes);
    free(argsCumulative);

    //the thread has finished its service to the client. it should die now.
    threadDec();
    pthread_exit(NULL);
}

void* getClientRequest(void* arg)
{
    while (1)
    {
        //wait for client to call my socket - THIS BLOCKS!
        int newsockfd = get_connection(clientfd);

        //got a connection - give this to a new thread to take care of it
        pthread_t thread;
        threadInc();
        if(pthread_create(&thread, NULL, serveClientRequest, (void*)&newsockfd))
        {
            printf("ERROR creating thread %d", 1);
            exit(-1);
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
            printf("terminate signal received\n");

            //set this flag on to let main thread know we're getting ready to die
            die = 1;

            //this thread is done
            threadDec();
            pthread_exit(NULL);
        }
    }
}

int rpcExecute()
{
    printf("rpcExecute\n");

    //create threads on stack
    pthread_t listener, getRequest;

    //make a thread for listening
    threadInc();
    if(pthread_create(&listener, NULL, listenForTerminate, NULL))
        return THREAD_ERROR;

    //make a thread for picking up client connection calls
    threadInc();
    if(pthread_create(&getRequest, NULL, getClientRequest, NULL))
        return THREAD_ERROR;

    //loop until we get terminate signal and all service threads are done
    while(die!=1 || threadcount > 2)
    {
        //when threadcount == 2 we can exit because:
        //thread1: this thread - this stays up
        //thread2: getRequest thread - this also stays up
    }

    //set a terminate response to binder
    int msglen = 0; //doesn't matter
    int msgtype = TERMINATE_SUCCESS;
    send(binderfd, &msglen, sizeof(msglen), 0);
    send(binderfd, &msgtype, sizeof(msgtype), 0);

    //bye binder
    close(binderfd);

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
    if(binder_address==NULL || binder_port<=0)
        return BINDER_NOT_FOUND;

    //connect to the binder
    binderfd=call_socket(binder_address, binder_port);
    if (binderfd < 0)
        return CALLSOCKET_ERROR;

    //send it to binder
    send(binderfd, &msglen, sizeof(msglen), MSG_WAITALL);
    send(binderfd, &msgtype, sizeof(msgtype), MSG_WAITALL);

    //bye binder
    close(binderfd);

    printf("rpcTerminate done\n");
    return SUCCESS;
}
