#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "rpc.h"

int rpcInit()
{
	int result=0;
	
	printf("rpcInit\n");
	return result;
}
int rpcCall(char* name, int* argTypes, void** args)
{
	int result=0;
	
	printf("rpcCall\n");
	return result;
}
int rpcRegister(char* name, int* argTypes, skeleton f)
{
	int result=0;
	
	printf("rpcRegister\n");
	return result;
}
int rpcExecute()
{
	int result=0;
	
	printf("rpcExecute\n");
	return result;
}
int rpcTerminate()
{
	int result=0;
	
	printf("rpcTerminate\n");
	return result;
}
