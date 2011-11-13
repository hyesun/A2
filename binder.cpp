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
#define SPORT   33333
#define CPORT   33334

//message struct
typedef struct
{
        string server_address;
        int port;
	string fn_name;
	unsigned int* argType;
	int argTypesLen;
}data_point;

vector<data_point> DataBase;

//helper functions

int function()
{
    return 0;
}

void server_register(int socketfd, int msglen)
{
    int success = SUCCESS;
    char server_address[MAXHOSTNAME+1];
    int port;
    char fn_name[MAXFNNAME+1];
    int argTypesLen = msglen-sizeof(server_address)-sizeof(port)-sizeof(fn_name); //yes
    unsigned int* argType = (unsigned int*)malloc(argTypesLen);

    //receive the message
    int checksum = msglen;
    checksum -= recv(socketfd, server_address, sizeof(server_address), 0);
    checksum -= recv(socketfd, &port, sizeof(port), 0);
    checksum -= recv(socketfd, fn_name, sizeof(fn_name), 0);
    checksum -= recv(socketfd, argType, argTypesLen, 0);

    if (checksum != 0)
      success = FAILURE;
    data_point a;
    a.server_address = server_address;
    a.port = port;
    a.fn_name = fn_name;
    a.argType = argType;
    a.argTypesLen = argTypesLen;
    DataBase.push_back(a);
    send(socketfd, &success, sizeof(success), 0);

    cout << "address: " << DataBase.back().server_address << endl;
    cout << "port: " << DataBase.back().port << endl;
    cout << "fn name: " << DataBase.back().fn_name << endl;
    for(int j=0; j<DataBase.back().argTypesLen/sizeof(int); j++)
    {
        cout << "argType[" << j << "]=" << DataBase.back().argType[j] << endl;
    }
}

//main function
int main()
{
    printf("binder\n");

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
    int socketfd;
    //multiplex
    for(;;)
    {
        counter++;
        read_fds = master; // copy it
        if (select(fdmax+1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }
        // run through the existing connections looking for data to read
        for(socketfd = 0; socketfd <= fdmax; socketfd++)
        {
            if (FD_ISSET(socketfd, &read_fds))
            {
                // we got one!!
                printf("we got one! i:%i\n", socketfd);
                if (socketfd == listener)
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
                    int success;
                    cout << endl << endl << "-------accept fn reg calls-----" << endl << endl;
                    //get first 8 bytes
                    int msglen;
                    int msgtype;
                    status = recv(socketfd, &msglen, sizeof(msglen), 0);
                    if (status > 0)
                      status = recv(socketfd, &msgtype, sizeof(msgtype), 0);

                    //check msgtype to see if from server
                    if (msgtype == REGISTER && status > 0)
                    {
                      server_register(socketfd,msglen);
                    }

                    if(status <= 0 )
                    {
                      cout << "terminating:" << socketfd << endl;
                      close(socketfd); // bye!
                      FD_CLR(socketfd, &master); // remove from master set
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
