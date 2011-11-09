rpc.o: rpc.c rpc.h
	gcc -g -c rpc.c
	ar crf lib/librpc.a rpc.o

clean: 
	rm -rf *o
	
