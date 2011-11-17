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
} data_point;

vector<data_point> DataBase;

//helper functions

int function()
{
    return 0;
}

int terminate_server()
{
    printf("terminate\n");
    return 0;
}

int database_lookup(string fn_name, unsigned int* argType, int argTypesLen)
{
    int index_found = -1;
    int fn_name_found = FAILURE;
    int arg_types_match = SUCCESS;
    for (int i = 0; i < DataBase.size(); i++)
    {
        if (fn_name == DataBase[i].fn_name)
        {
            index_found = i;
            fn_name_found = SUCCESS;
            for (int j = 0; j < argTypesLen / sizeof(int); j++)
            {
                if (argType[j] != DataBase[index_found].argType[j])
                    arg_types_match = FAILURE;
            }
        }
    }
    if (fn_name_found == SUCCESS && arg_types_match == SUCCESS)
        return index_found;
    else
        return FAILURE;
}

void binder_register(int socketfd, int msglen)
{
    int success = SUCCESS;
    char server_address[MAXHOSTNAME + 1];
    int port;
    char fn_name[MAXFNNAME + 1];
    int argTypesLen = msglen - sizeof(server_address) - sizeof(port)
            - sizeof(fn_name); //yes
    unsigned int* argType = (unsigned int*) malloc(argTypesLen);

    //receive the message
    int checksum = msglen;
    checksum -= recv(socketfd, server_address, sizeof(server_address), 0);
    checksum -= recv(socketfd, &port, sizeof(port), 0);
    checksum -= recv(socketfd, fn_name, sizeof(fn_name), 0);
    checksum -= recv(socketfd, argType, argTypesLen, 0);

    if (checksum != 0)
        success = FAILURE;
    //check if same function from same server already exists
    //and over write if true
    int index = database_lookup(fn_name, argType, argTypesLen);
    if (index != -1 && DataBase[index].server_address == server_address
            && DataBase[index].port == port)
    {
        //over write
        DataBase[index].fn_name = fn_name;
        DataBase[index].argType = argType;
        DataBase[index].argTypesLen = argTypesLen;
    }
    else
    {
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
        for (int j = 0; j < DataBase.back().argTypesLen / sizeof(int); j++)
        {
            cout << "argTypeLen: " << DataBase.back().argTypesLen << endl;
            cout << "argType[" << j << "]=" << DataBase.back().argType[j]
                    << endl;
        }
    }
}

void binder_service_client(int socketfd, int msglen)
{
    cout << "binder lookup" << endl;
    int success = SUCCESS;
    char fn_name[MAXFNNAME + 1];
    int argTypesLen = msglen - sizeof(fn_name); //yes
    unsigned int* argType = (unsigned int*) malloc(argTypesLen);

    //receive the message
    int checksum = msglen;
    checksum -= recv(socketfd, fn_name, sizeof(fn_name), 0);
    checksum -= recv(socketfd, argType, argTypesLen, 0);

    if (checksum == 0)
    {
        cout << "binder side (got request): fn request: " << fn_name << endl;

        for (int j = 0; j < argTypesLen / sizeof(int); j++)
        {
            cout << "binder side (got request):argType[" << j << "]="
                    << argType[j] << endl;
        }

        int found = FAILURE;

        int index_found = -1;
        //lookup returns the index of found
        index_found = database_lookup((string) fn_name, argType, argTypesLen);

        if (index_found != FAILURE)
        {
            cout << "SERVER FUNCTION FOUND" << endl;
            const char* server_address =
                    DataBase[index_found].server_address.c_str();
            int port = DataBase[index_found].port;
            int sendmsglen = MAXHOSTNAME + s_char + sizeof(port);
            int sendmsgtype = LOC_SUCCESS;
            int sendchecksum = sendmsglen + sizeof(sendmsgtype)
                    + sizeof(sendmsglen);

            sendchecksum -= send(socketfd, &sendmsglen, sizeof(sendmsglen), 0);
            sendchecksum
                    -= send(socketfd, &sendmsgtype, sizeof(sendmsgtype), 0);
            sendchecksum -= send(socketfd, server_address, MAXHOSTNAME + 1, 0);
            sendchecksum -= send(socketfd, &port, sizeof(port), 0);
            cout << "msglen: " << sendmsglen << endl;
            cout << "sendmsgtype: " << sendmsgtype << endl;
            cout << "address: " << server_address << endl;
            cout << "port: " << port << endl;

            //put the serviced function to the end
            data_point temp;
            temp.fn_name = DataBase[index_found].fn_name;
            temp.server_address = DataBase[index_found].server_address;
            temp.port = DataBase[index_found].port;
            temp.argType = DataBase[index_found].argType;
            temp.argTypesLen = DataBase[index_found].argTypesLen;
            DataBase.erase(DataBase.begin() + index_found);
            DataBase.push_back(temp);

            if (checksum != 0)
            {
                cout << "Error sending back LOC SUCCESS" << endl;
            }

        }
        else
        {
            cout << "SERVER FUNCTION NOT FOUND" << endl;
            int msglen = MAXFNNAME + s_char + argTypesLen * s_int;
            int msgtype = LOC_FAILURE;
            int checksum = msglen + sizeof(msgtype) + sizeof(msglen);
        }
    }

}

//main function
int main()
{
    printf("binder\n");

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
                printf("we got one! i:%i\n", socketfd);
                if (socketfd == listener)
                {
                    printf("i is listener\n");
                    newfd = accept(listener, NULL, NULL);
                    cout << newfd << " is new socket" << endl;

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
                    cout << endl << endl << "-------accept fn reg calls-----"
                            << endl << endl;
                    //get first 8 bytes
                    int msglen;
                    int msgtype;
                    status = recv(socketfd, &msglen, sizeof(msglen), 0);
                    if (status > 0)
                        status = recv(socketfd, &msgtype, sizeof(msgtype), 0);

                    //check msgtype to see if from server
                    if (msgtype == REGISTER && status > 0)
                    {
                        binder_register(socketfd, msglen);
                    }
                    else if (msgtype == LOC_REQUEST)
                    {
                        binder_service_client(socketfd, msglen);
                    }
                    else if (msgtype == TERMINATE)
                    {
                        terminate_server();
                    }

                    if (status <= 0)
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
