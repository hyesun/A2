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
#include <arpa/inet.h>
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
//#define ADDRESS "mef-linux010.student"//"stephen-Rev-1-0"
#define ADDRESS "stephen-Rev-1-0"
#define BPORT   33337
#define SPORT   0

//message types
enum message_type
{
    REGISTER,
    LOC_REQUEST,
    LOC_SUCCESS,
    LOC_FAILURE
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
    struct in_addr **addr_list;

    //get host info
    host = gethostbyname(hostname);
    addr_list = (struct in_addr **)host->h_addr_list;
    cout << "host name: " << inet_ntoa(*addr_list[0]) <<endl;
    if (host == NULL)
    {
        printf("gethost error\n");
        return FAILURE;
    }

    //config socket
    memset(&my_addr, 0, sizeof(my_addr));   //clean memory
    memcpy((char *) &my_addr.sin_addr, host->h_addr, host->h_length);
    //bcopy((char *)host->h_addr,(char *)&my_addr.sin_addr.s_addr,host->h_length);
    cout << "host name in my_addr:" << inet_ntoa(my_addr.sin_addr) << endl;
    my_addr.sin_family =host->h_addrtype;
    my_addr.sin_port = htons((u_short)portnum);
    //get socket file descriptor
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        printf("socket error: %i\n", sockfd);
        return sockfd;
    }
    cout << "socket result: " << sockfd << endl;
    //connect
    result = connect(sockfd, (struct sockaddr*)&my_addr, sizeof my_addr);
    cout << "connect result: " << result << endl;
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
    char fn_server_address[MAXHOSTNAME + 1];
    int fn_server_port;
    binderfd=call_socket((char*)ADDRESS, BPORT);
    if (binderfd < 0)
    {
        printf("call socket error: %i\n", binderfd);
        return binderfd;
    }

    int argTypesLen = 0;
    while(*(argTypes+argTypesLen) != 0)
    {
        argTypesLen++;
    }
    argTypesLen++;
    //LOC_REQUEST
    int msglen = MAXFNNAME+s_char + argTypesLen*s_int;
    int msgtype = LOC_REQUEST;
    int checksum = msglen + sizeof(msgtype) + sizeof(msglen);
    cout << "checksum:" << checksum << endl;
    cout << "binderfd: " << binderfd << endl;
    checksum -= send(binderfd, &msglen, sizeof(msglen), 0);
    checksum -= send(binderfd, &msgtype, sizeof(msgtype), 0);
    checksum-=send(binderfd, name, MAXFNNAME+s_char, 0);
    checksum-=send(binderfd, argTypes, argTypesLen*s_int, 0);
    cout << "LOC REQUEST OF FN:" << name << endl;
    cout << "argTypesLen:" << argTypesLen << endl;
    for(int j=0; j<argTypesLen; j++)
    {
        cout << "LOC QUEST OF argType[" << j << "]=" << (unsigned int)argTypes[j] << endl;
    }
    cout << "checksum at end of LOC_REQUEST:" << checksum << endl;
    if (checksum != 0)
      return FAILURE;
    else//LOC_SUCCESS
    {
        int status = recv(binderfd, &msglen, sizeof(msglen), 0);
        if (status > 0)
            status = recv(binderfd, &msgtype, sizeof(msgtype), 0);
        cout << "msglen: " << msglen << endl;
        cout << "sendmsgtype: " << msgtype << endl;
        if (msgtype == LOC_SUCCESS)
        {
            cout << "LOC SUCCESS!!" << endl;
            checksum = msglen;
            checksum -= recv(binderfd, fn_server_address, sizeof(fn_server_address), 0);
            cout << "LOC SUCCESS HERE!!" << endl;
            cout << "client gets server_add:" << fn_server_address << endl;
            checksum -= recv(binderfd, &fn_server_port, sizeof(fn_server_port), 0);
            cout << "client gets server_add:" << fn_server_address << endl;
            cout << "client gets server_port_num:" << fn_server_port << endl;
            if (checksum !=0)
              return FAILURE;
        }
    }
    close(binderfd);

    printf("rpcCall\n");
    return SUCCESS;
}

int rpcRegister(char* name, int* argTypes, skeleton f)
{
    printf("rpcRegister\n");

    //count argTypes length
    int argTypesLen = 0;
    while(*(argTypes+argTypesLen) != 0)
    {
        argTypesLen++;
    }
    argTypesLen++;

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

    //double check with binder to make sure it was successful
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
    pause();
    printf("rpcExecute\n");
    return SUCCESS;
}

int rpcTerminate()
{
    printf("rpcTerminate\n");
    return SUCCESS;
}
