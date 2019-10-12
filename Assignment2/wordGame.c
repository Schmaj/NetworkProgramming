#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>

#include <signal.h>
#include <fcntl.h>

#include <errno.h>


// number of clients allowed
#define BACKLOG 5

int main(int argc, char* argv[]){

	// check for correct number of arguments
	if(argc < 4){
		printf("Insufficient arguments\n");
		printf("Usage: ./word_guess.out [seed] [port] [dictionary_file] [longest_word_length]");
		return 0;
	}

	// initialize variables from arguments

	// seed for randomizing dictionary
	int seed = atoi(argv[1]);
	// port for listening socket
	int port = atoi(argv[2]);
	// name of dictionary file
	char* dictFile = calloc(strlen(argv[3])+1, sizeof(char));
	strcpy(dictFile, argv[3]);
	// length of longest word in dictionary
	int maxWordLen = atoi(argv[4]);

	// file descriptor listening for new connections
	int listenFd = socket(PF_INET, SOCK_STREAM, 0);

	if(listenFd < 0){
		perror("socket() failed\n");
		return EXIT_FAILURE;
	}

	// holds address and port of server
	struct sockaddr_in server;

	// zero out sockaddr
	memset(&server, 0, sizeof(struct sockaddr_in));

	// use IPv4
	server.sin_family = AF_INET;
	// allow any ip address
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	// initialize to min port in port range
	server.sin_port = htons(port);

	// size of struct for bind()
	int len = sizeof(struct sockaddr_in);

	// bind listening server to address 
	if(bind(listenFd, (struct sockaddr*)&server, len) != 0){
		perror("bind() failed\n");
		return EXIT_FAILURE;
	}

	// set listening socket to listening state
	if(listen(listenFd, BACKLOG) != 0){
		perror("listen() failed\n");
		return EXIT_FAILURE;
	}

	while(1){

	}




}