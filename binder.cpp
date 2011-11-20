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

//global variables
int terminate_flag = 0;
vector<data_point> DataBase;
vector<int> SocketDataBase;

//================================================================
//  HELPER FUNCTIONS
//================================================================

int lookupDatabase(string fn_name, unsigned int* argType, int argTypesLen)
{
    int index_found = -1;
    int arg_types_match, arg_types_len_match;
    int intoutbits = 0;
    for (int i = 0; i < DataBase.size(); i++)
    {
        arg_types_match = SUCCESS;
        if (fn_name == DataBase[i].fn_name)
        {
            arg_types_len_match = SUCCESS;
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
            else
                arg_types_len_match = FAILURE;
        }
    }

    //return error codes: from most specific to least specific

    if (arg_types_match == FAILURE)
        return ARGTYPES_MISMATCH;

    if(arg_types_len_match == FAILURE)
        return ARGTYPESLEN_MISMATCH;

    return SERVER_FN_NOT_FOUND;
}

void fnRegister(int socketfd)
{
    int msglen, msgtype, result, server_port;
    char fn_name[MAXFNNAME + 1];
    char server_address[MAXHOSTNAME + 1];

    //first recv msglen
    recv(socketfd, &msglen, sizeof(msglen), 0);

    //calculate
    int argTypesLen = msglen - sizeof(server_address) - sizeof(server_port) - sizeof(fn_name);
    unsigned int* argType = (unsigned int*) malloc(argTypesLen);

    //recv the rest
    recv(socketfd, server_address, sizeof(server_address), 0);
    recv(socketfd, &server_port, sizeof(server_port), 0);
    recv(socketfd, fn_name, sizeof(fn_name), 0);
    recv(socketfd, argType, argTypesLen, 0);

    //check if same function from same server already exists and over write if true
    int index = lookupDatabase(fn_name, argType, argTypesLen);
    if (index >=0 && DataBase[index].server_address
            == server_address && DataBase[index].port == server_port)
    {
        //overwrite
        msgtype = REGISTER_WARNING;
        DataBase[index].server_address = server_address;
        DataBase[index].port = server_port;
        DataBase[index].fn_name = fn_name;
        DataBase[index].argType = argType;
        DataBase[index].argTypesLen = argTypesLen;
        DataBase[index].server_socket_fd = socketfd;
    }
    else
    {
        msgtype = REGISTER_SUCCESS;
        data_point a;
        a.server_address = server_address;
        a.port = server_port;
        a.fn_name = fn_name;
        a.argType = argType;
        a.argTypesLen = argTypesLen;
        a.server_socket_fd = socketfd;
        DataBase.push_back(a);
    }
    send(socketfd, &msgtype, sizeof(msgtype), 0);
}

void fnLocator(int socketfd)
{
    int success = SUCCESS;
    char fn_name[MAXFNNAME + 1];
    int found = FAILURE;
    int index_found = -1;
    int msglen;

    //first get msglen
    recv(socketfd, &msglen, sizeof(msglen), 0);

    //calculate
    int argTypesSize = msglen - sizeof(fn_name);
    unsigned int* argTypes = (unsigned int*) malloc(argTypesSize);

    //recv the rest
    recv(socketfd, fn_name, sizeof(fn_name), 0);
    recv(socketfd, argTypes, argTypesSize, 0);

    //lookup returns the index of found
    index_found = lookupDatabase((string)fn_name, argTypes, argTypesSize);

    if (index_found >= 0)
    {
        //SERVER FUNCTION FOUND
        int sendmsgtype = LOC_SUCCESS;
        const char* server_address = DataBase[index_found].server_address.c_str();
        int server_port = DataBase[index_found].port;

        send(socketfd, &sendmsgtype, sizeof(sendmsgtype), 0);
        send(socketfd, server_address, MAXHOSTNAME + 1, 0);
        send(socketfd, &server_port, sizeof(server_port), 0);

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
    }
    else
    {
        //SERVER FUNCTION NOT FOUND
        int msgtype = LOC_FAILURE;
        int reasonCode = index_found;
        send(socketfd, &msgtype, sizeof(msgtype), 0);
        send(socketfd, &reasonCode, sizeof(reasonCode), 0);
    }
}

void terminateServers()
{
    int msgtype = TERMINATE;
    int recvmsgtype = -1;

    for (int i = 0; i < SocketDataBase.size(); i++)
    {
        send(SocketDataBase[i], &msgtype, sizeof(msgtype), 0);
        recv(SocketDataBase[i], &recvmsgtype, sizeof(recvmsgtype), 0);
        if (recvmsgtype != TERMINATE_SUCCESS)
            return;
    }

    terminate_flag = 1;
}

//================================================================
//  MAIN FUNCTION
//================================================================

int main()
{
    fd_set master; //master file descriptor list
    fd_set read_fds; //temp file descriptor list for select()
    int fdmax; //maximum file descriptor number
    int newfd; //newly accept()ed socket descriptor

    //clear the sets
    FD_ZERO(&master);
    FD_ZERO(&read_fds);

    //establish a listening socket
    int listener = establish(BPORT, 1);
    if (listener < 0)
        return ESTABLISH_ERROR;

    FD_SET(listener, &master);
    fdmax = listener; // so far, it's this one

    //================================================================
    //  MULTIPLEXING
    //================================================================

    for (;;)
    {
        read_fds = master; // copy it

        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
            return BINDER_SELECT_ERROR;

        // run through the existing connections looking for data to read
        for (int socketfd = 0; socketfd <= fdmax; socketfd++)
        {
            if (FD_ISSET(socketfd, &read_fds))
            {
                if (socketfd == listener) //a new connection
                {
                    newfd = accept(listener, NULL, NULL);

                    if (newfd < 0)
                        return BINDER_ACCEPT_ERROR;
                    else
                    {
                        FD_SET(newfd, &master); //add to master set
                        if (newfd > fdmax)
                            fdmax = newfd; //update max
                    }
                }
                else //send/recv connection
                {
                    int status, success, msglen, msgtype;
                    int socket_add_flag = 1;

                    status = recv(socketfd, &msgtype, sizeof(msgtype), 0);

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
                        for (int i = 0; i < SocketDataBase.size(); i++)
                        {
                            if (SocketDataBase[i] == socketfd)
                            {
                                SocketDataBase.erase(SocketDataBase.begin() + i);
                                i--;
                            }
                        }
                        close(socketfd); // bye!
                        FD_CLR(socketfd, &master); // remove from master set
                        if (terminate_flag == 1 && SocketDataBase.size() == 0)
                            exit(0);
                    }
                    else if (msgtype == REGISTER)
                    {
                        fnRegister(socketfd);

                        for (int i = 0; i < SocketDataBase.size(); i++)
                        {
                            if (SocketDataBase[i] == socketfd)
                                socket_add_flag = 0;
                        }
                        if (socket_add_flag == 1)
                            SocketDataBase.push_back(socketfd);
                    }
                    else if (msgtype == LOC_REQUEST)
                    {
                        fnLocator(socketfd);
                    }
                    else if (msgtype == TERMINATE)
                    {
                        terminateServers();
                    }
                }
            }
        }
    }
    return 0;
}
