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

//message struct
typedef struct
{
	string procedure;
	string location;
}data_point;

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
    int listener = establish(PORT);
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
                        char* buffer = (char*)malloc(100);
                        status = recv(i, buffer, 100, 0);
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
	

	//accept function register calls from server
    int msglen;
    int msgtype;
    recv(i, (char*)&msglen, 4, 0);
    recv(i, (char*)&msgtype, 4, 0);

	message_sb msg;
	recv(i, (char*)&msg, msglen, 0);

    cout << "fn name is: " << msg.fn_name << endl;

    printf("binder done\n");
    return 0;
}
