#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>

#include "rpc.h"
#include <iostream>

#include <vector>
#include <string>

#define ADDRESS "hyesun-ubuntu";
#define SPORT   3333
#define CPORT   3334
#define BACKLOG 5       //max # of queued connects
#define MAXHOSTNAME 100

#define NUMTHREADS 2
#define SERVER_THREAD 0
#define CLIENT_THREAD 1

#define MAX_NUM_REGISTERS 100

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
    int port;
    string fn_name;
    string ip_address;
    int* argTypes;
} message_sb;

int main()
{
    printf("binder\n");

    //create connection socket for server
    int serverfd = establish(SPORT);
    if (serverfd < 0)
    {
        printf("establish error: %i\n", serverfd);
        return serverfd;
    }
    int newserverfd = get_connection(serverfd);
    if (newserverfd < 0)
    {
        printf("get_connection error: %i\n", newserverfd);
        return newserverfd;
    }

    //accept function register calls from server
    char* a = (char*)malloc(3);
    recv(newserverfd, a, 3, 0);

    int msglen;
    int msgtype;
    recv(newserverfd, (char*)&msglen, 4, 0);
    recv(newserverfd, (char*)&msgtype, 4, 0);

    printf("msglen: %i\n", msglen);
    printf("msgtype: %i\n", msgtype);


    char *msgchar = (char*)malloc(msglen);
    int bytes_read = recv(newserverfd, msgchar, msglen, 0);

    printf("message received: %i\n", bytes_read);

    message_sb *msg = (message_sb*)msgchar;
    cout << "fn name is: " << msg->fn_name << endl;

    printf("binder done\n");
    return 0;
}
