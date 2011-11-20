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

//message struct
typedef struct
{
        string server_address;
        int port;
        string fn_name;
        unsigned int* argType;
        int argTypesLen;
        int server_socket_fd;
} data_point;

vector<data_point> DataBase;

vector<int> SocketDataBase;

int terminate_flag = 0;
//

//helper functions

int terminate_server()
{
    int serverfd;
    int msglen=0;
    int msgtype=TERMINATE;
    int recvmsglen=0;
    int recvmsgtype = -1;
    terminate_flag = 1;
    for (int i=0; i < SocketDataBase.size(); i++)
    {
      serverfd = SocketDataBase[i];
      send(serverfd, &msglen, sizeof(msglen), 0);
      send(serverfd, &msgtype, sizeof(msgtype), 0);
      recv(serverfd, &msglen, sizeof(msglen), 0);
      recv(serverfd, &recvmsgtype, sizeof(recvmsgtype),0);
      if(recvmsgtype != TERMINATE_SUCCESS)
          terminate_flag = 0;
    }
    return 0;
}

int database_lookup(string fn_name, unsigned int* argType, int argTypesLen)
{
    int index_found = -1;
    int arg_types_match;
    int intoutbits = 0;
    for (int i = 0; i < DataBase.size(); i++)
    {
        arg_types_match = SUCCESS;
        if (fn_name == DataBase[i].fn_name)
        {
            index_found = i;
            if (argTypesLen == DataBase[index_found].argTypesLen)
            {
                for (int j = 0; (j < argTypesLen / sizeof(int)) && (arg_types_match == SUCCESS); j++)
                {
                    //compare output/input
                    if(argType[j] >> 30 != DataBase[index_found].argType[j] >> 30)
                        arg_types_match = FAILURE;
                    if (getArgType((int*)(argType+j)) != getArgType((int*)(DataBase[index_found].argType+j)))
                        arg_types_match = FAILURE;
                    if (getArgType((int*)(argType+j)) > getArgType((int*)(DataBase[index_found].argType+j)))
                        arg_types_match = FAILURE;
                }
                if (arg_types_match == SUCCESS)
                    return index_found;
            }
        }
    }
    return SERVER_FN_NOT_FOUND;
}

int binder_register(int socketfd, int msglen)
{
    int result;
    char server_address[MAXHOSTNAME + 1];
    int port;
    char fn_name[MAXFNNAME + 1];
    int argTypesLen = msglen - sizeof(server_address) - sizeof(port)
            - sizeof(fn_name); //yes
    unsigned int* argType = (unsigned int*) malloc(argTypesLen);

    //receive the message
    recv(socketfd, server_address, sizeof(server_address), 0);
    recv(socketfd, &port, sizeof(port), 0);
    recv(socketfd, fn_name, sizeof(fn_name), 0);
    recv(socketfd, argType, argTypesLen, 0);
    //check if same function from same server already exists
    //and over write if true
    int index = database_lookup(fn_name, argType, argTypesLen);
    if (index != SERVER_FN_NOT_FOUND && DataBase[index].server_address == server_address
            && DataBase[index].port == port)
    {
        //over write
        result = DUPLICATE_FN;
        DataBase[index].server_address = server_address;
        DataBase[index].port = port;
        DataBase[index].fn_name = fn_name;
        DataBase[index].argType = argType;
        DataBase[index].argTypesLen = argTypesLen;
        DataBase[index].server_socket_fd = socketfd;
        send(socketfd, &result, sizeof(result), 0);
        return DUPLICATE_FN;
    }
    else
    {
        result = SUCCESS;
        data_point a;
        a.server_address = server_address;
        a.port = port;
        a.fn_name = fn_name;
        a.argType = argType;
        a.argTypesLen = argTypesLen;
        a.server_socket_fd = socketfd;
        DataBase.push_back(a);
        send(socketfd, &result, sizeof(result), 0);
        return SUCCESS;
    }
}

int binder_service_client(int socketfd, int msglen)
{
    int success = SUCCESS;
    char fn_name[MAXFNNAME + 1];
    int argTypesLen = msglen - sizeof(fn_name); //yes
    unsigned int* argType = (unsigned int*) malloc(argTypesLen);

    //receive the message
    recv(socketfd, fn_name, sizeof(fn_name), 0);
    recv(socketfd, argType, argTypesLen, 0);

    int found = FAILURE;

    int index_found = -1;
    //lookup returns the index of found
    index_found = database_lookup((string) fn_name, argType, argTypesLen);

    if (index_found != SERVER_FN_NOT_FOUND)
    {
        //SERVER FUNCTION FOUND
        const char* server_address =
                DataBase[index_found].server_address.c_str();
        int port = DataBase[index_found].port;
        int sendmsglen = MAXHOSTNAME+sizeof(char) + sizeof(port);
        int sendmsgtype = LOC_SUCCESS;

        send(socketfd, &sendmsglen, sizeof(sendmsglen), 0);
        send(socketfd, &sendmsgtype, sizeof(sendmsgtype), 0);
        send(socketfd, server_address, MAXHOSTNAME + 1, 0);
        send(socketfd, &port, sizeof(port), 0);

        //put the serviced function to the end
        data_point temp;
        temp.fn_name = DataBase[index_found].fn_name;
        temp.server_address = DataBase[index_found].server_address;
        temp.port = DataBase[index_found].port;
        temp.server_socket_fd = DataBase[index_found].server_socket_fd;
        temp.argType = DataBase[index_found].argType;
        temp.argTypesLen = DataBase[index_found].argTypesLen;
        DataBase.erase(DataBase.begin() + index_found);
        DataBase.push_back(temp);
        return SUCCESS;
    }
    else
    {
        //SERVER FUNCTION NOT FOUND
        int msglen = MAXFNNAME+sizeof(char) + argTypesLen*sizeof(int);
        int msgtype = SERVER_FN_NOT_FOUND;
        int reasonCode = FAILURE;
        send(socketfd, &msglen, sizeof(msglen), 0);
        send(socketfd, &msgtype, sizeof(msgtype), 0);
        send(socketfd, &reasonCode, sizeof(reasonCode), 0);
        return SERVER_FN_NOT_FOUND;
    }
}

//main function
int main()
{
    //printf("binder\n");

    fd_set master; // master file descriptor list
    fd_set read_fds; // temp file descriptor list for select()
    int fdmax; // maximum file descriptor number

    //create connection socket for server
    FD_ZERO(&master); // clear the master and temp sets
    FD_ZERO(&read_fds);
    int listener = establish(BPORT, 1);
    int newfd; // newly accept()ed socket descriptor
    if (listener < 0)
    {
        printf("establish error (binder): %i\n", listener);
        return listener;
    }
    FD_SET(listener, &master);
    fdmax = listener; // so far, it's this one
    int counter = 0;
    int socketfd;
    //multiplex
    for (;;)
    {
        counter++;
        read_fds = master; // copy it
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }
        // run through the existing connections looking for data to read
        for (socketfd = 0; socketfd <= fdmax; socketfd++)
        {
            if (FD_ISSET(socketfd, &read_fds))
            {
                // we got one!!
                if (socketfd == listener)
                {
                    newfd = accept(listener, NULL, NULL);

                    if (newfd == -1)
                    {
                        perror("accept");
                    }
                    else
                    {
                        FD_SET(newfd, &master); // add to master set
                        if (newfd > fdmax)
                        { // keep track of the max
                            fdmax = newfd;
                        }
                    }
                }
                else
                {
                    int status;
                    int success;
                    //get first 8 bytes
                    int msglen;
                    int msgtype;
                    int socket_add_flag = 1;
                    status = recv(socketfd, &msglen, sizeof(msglen), 0);
                    if (status > 0)
                        status = recv(socketfd, &msgtype, sizeof(msgtype), 0);

                    //check msgtype to see if from server
                    if (msgtype == REGISTER && status > 0)
                    {
                        binder_register(socketfd, msglen);
                        for (int i=0; i< SocketDataBase.size(); i++)
                        {
                            if (SocketDataBase[i] == socketfd)
                              socket_add_flag = 0;
                        }
                        if (socket_add_flag == 1)
                          SocketDataBase.push_back(socketfd);
                    }
                    else if (msgtype == LOC_REQUEST && status > 0)
                    {
                        binder_service_client(socketfd, msglen);
                    }
                    else if (msgtype == TERMINATE && status > 0)
                    {
                        terminate_server();
                    }

                    if (status <= 0)
                    {
                        //get rid of the datapoint if socketfd is in the database of servers
                        for (int i = 0; i < DataBase.size(); i++)
                        {
                            if (DataBase[i].server_socket_fd == socketfd)
                            {
                                DataBase.erase(DataBase.begin() + i);
                                i--;
                            }
                        }
                        //get rid of socketfd in socket database
                        for (int i =0; i < SocketDataBase.size(); i++)
                        {
                          if (SocketDataBase[i] == socketfd)
                          {
                              SocketDataBase.erase(SocketDataBase.begin() + i);
                              i--;
                          }
                        }
                        close(socketfd); // bye!
                        FD_CLR(socketfd, &master); // remove from master seta
                        if(terminate_flag == 1 && SocketDataBase.size() == 0)
                          exit(0);
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
