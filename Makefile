all: client server

client: rpc.o client.o 
	g++ rpc.o client.o -L./lib -lrpc -o client
	
server: rpc.o server.o
	g++ rpc.o server.o server_functions.o server_function_skels.o -L./lib -lrpc -o server

rpc.o: rpc.c
	gcc -g -c rpc.c
	ar crf lib/librpc.a rpc.o

client.o: client1.c
	gcc -g -c client1.c -o client.o

server.o: server.c server_functions.c server_function_skels.c
	gcc -g -c server.c server_functions.c server_function_skels.c

clean: 
	rm -rf *o client server ./lib/librpc.a
