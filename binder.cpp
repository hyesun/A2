using namespace std;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <string>
#include "rpc.h"
#include "rpc.cpp"

//perm defines
#define NUMTHREADS 2
#define SERVER_THREAD 0
#define CLIENT_THREAD 1
#define MAX_NUM_REGISTERS 100

//temp defines
#define ADDRESS "hyesun-ubuntu";
#define SPORT   3333
#define CPORT   3334

//message struct
typedef struct
{
	string procedure;
	string location;
}data_point;

//helper functions

int function()
{
    return 0;
}

//main function
int main()
{
    printf("binder\n");

    /*vector<data_point> testing;
    cout << "size of the new vector:" << testing.size() << endl;
    data_point first;
    first.location = "first location";
    first.procedure = "first procedure";
    testing.push_back(first);
    data_point second;
    second.location = "second location";
    second.procedure = "second procedure";
    testing.push_back(second);
    second.location = "sdfsd sdfs";
    second.procedure = "sdf sf";
    cout << "size of the new vector:" << testing.size() << endl;
    for (int i=0; i < testing.size(); i++)
    {
      cout << i << "  location:" << testing[i].location
          << " procedure:" << testing[i].procedure << endl;
    }*/

    fd_set master;    // master file descriptor list
    fd_set read_fds;  // temp file descriptor list for select()
    int fdmax;        // maximum file descriptor number

    //create connection socket for server
    FD_ZERO(&master);    // clear the master and temp sets
    FD_ZERO(&read_fds);
    int listener = establish(SPORT);
    int newfd;        // newly accept()ed socket descriptor
    if (listener < 0)
    {
        printf("establish error (binder): %i\n", listener);
        return listener;
    }
    FD_SET(listener, &master);
    fdmax = listener; // so far, it's this one
    int counter=0;
    int i;
    //multiplex
    for(;;)
    {
        counter++;
        read_fds = master; // copy it
        printf("here1!!\n");
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }
        printf("here2!!\n");
        // run through the existing connections looking for data to read
        for(i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                // we got one!!
                printf("we got one! i:%i\n", i);
                if (i == listener)
                {
                    printf("i is listener\n");
                    newfd = accept(listener,NULL, NULL);
                    cout << newfd << " is new socket" << endl;

                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax)
                        {    // keep track of the max
                            fdmax = newfd;
                        }
                    }
                }
                else
                {
                    int status;


                    cout << endl << endl << "-------accept fn reg calls-----" << endl << endl;

                    //get first 8 bytes
                    int msglen;
                    int msgtype;
                    recv(i, &msglen, sizeof(msglen), 0);
                    recv(i, &msgtype, sizeof(msgtype), 0);

                    //get message variable ready
                    char server_address[MAXHOSTNAME+1];
                    int port;
                    char fn_name[MAXFNNAME+1];
                    int argTypesLen = msglen-sizeof(server_address)-sizeof(port)-sizeof(fn_name); //yes
                    unsigned int* argType = (unsigned int*)malloc(argTypesLen);

                    //receive the message
                    recv(i, server_address, sizeof(server_address), 0);
                    recv(i, &port, sizeof(port), 0);
                    recv(i, fn_name, sizeof(fn_name), 0);
                    recv(i, argType, argTypesLen, 0);

                    cout << "msglen: " << msglen << endl;
                    cout << "msgtype: " << msgtype << endl;
                    cout << "address: " << server_address << endl;
                    cout << "port: " << port << endl;
                    cout << "fn name: " << fn_name << endl;

                    for(int j=0; j<argTypesLen/sizeof(int); j++)
                    {
                        cout << "argType[" << j << "]=" << argType[j] << endl;
                    }

                    cout << endl << endl << "-------------------------------" << endl << endl;

                    //pause here so we can check output
                    getchar();

                    //do shit here
                    /*
                    if(status1 <= 0 )
                    {
                        close(i); // bye!
                        FD_CLR(i, &master); // remove from master set
                    }*/

                    if(status <= 0 )
                    {
                      cout << "terminating:" << i << endl;
                      close(i); // bye!
                      FD_CLR(i, &master); // remove from master set
                    }
                    //cleanup
                    //free(buffer);
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for(;;)--and you thought it would never end!
    //create arrays
	
    printf("binder done\n");
    return 0;
}
