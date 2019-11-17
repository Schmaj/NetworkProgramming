default: server.out client.out

client.out: client.c
	gcc client.c -Wall -o client.out -lm -g

server.out: server.c
	gcc server.c -Wall -o server.out -lm -pthread -g
