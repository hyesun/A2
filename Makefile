all: client server binder

client: client.o rpc.o 
	g++ client.o librpc.a -o client
	
server: server.o rpc.o 
	g++ server.o server_functions.o server_function_skels.o librpc.a -o server

binder: binder.o rpc.o
	g++ binder.o librpc.a -o binder
	
client.o: client1.c
	g++ -w -g -c client1.c -o client.o

server.o: server.c server_functions.c server_function_skels.c
	g++ -w -g -c server.c server_functions.c server_function_skels.c

binder.o: binder.cpp
	g++ -g -c binder.cpp

rpc.o: rpc.cpp
	g++ -g -c rpc.cpp
	ar crf librpc.a rpc.o

clean: 
	rm -rf *o client server binder librpc.a
