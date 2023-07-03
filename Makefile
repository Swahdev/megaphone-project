CC = gcc -Wall -pthread Server.c memory.c -o server
CD = gcc -Wall -pthread Client.c fichier.c -o client
all:
	$(CC) -o server
	$(CD) -o client
clean :
	rm -f *.o server
	rm -f *.o client
