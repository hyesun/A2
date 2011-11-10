#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "rpc.h"

#define ADDRESS "hyesun-ubuntu";
#define SPORT   5555
#define CPORT   6666
#define BACKLOG 5       //max # of queued connects
#define MAXHOSTNAME 100

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

    printf("binder done\n");
    return 0;
}
