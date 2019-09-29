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


#define MAX_BUFFER 250
#define MAX_BACKLOG 32


int interactWithClient(int fd, struct sockaddr* client, char* buf){

	int bytesRead = 0;
	while(1){
		// read from stdin and place in buf
		bytesRead = read(0, buf, MAX_BUFFER);

		// 0 bytes means closed connection
		if(bytesRead == 0){
			printf("str_cli: client disconnected\n");
			return 1;
		}

		// null terminate string, but at an index past what will be sent to the client
		// string functions will be used
		buf[MAX_BUFFER] = '\0';

		// check for end of file character
		if(strchr(buf, -1) != NULL){
			printf("Shutting down due to EOF\n");
			return 0;
		}

		if(sendto(fd, buf, bytesRead, 0, client, sizeof(struct sockaddr)) == -1){
			perror("ERROR: sendto() failed\n");
		}

		

	}

}

int main(int argc, char* argv[]){



	// check number of arguments
	if(argc < 2){
		printf("Insufficient arguemnts\n");
		printf("Usage: ./serv.out port\n");
		return 0;
	}

	// initialize port
	int port = atoi(argv[1]);
	port += 9877;

	// struct to hold ip address and port of udp server waiting for client
	struct sockaddr_in server;

	// use IPv4
	server.sin_family = AF_INET;
	// allow any ip address
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	// initialize to min port in port range
	server.sin_port = htons(port);

	// file descriptor referring to a socket listening for the next UDP client
	// creates a socket, IPv4, UDP, default protocol
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if(fd < 0){
		perror("socket() failed\n");
		return EXIT_FAILURE;
	}

	// bind the socket to any ip address, and the next port in range, allowing it to be reached
	int rc = bind(fd, (struct sockaddr*)&server, sizeof(server));
	if(rc < 0 ){
		perror("bind() failed\n");
		return EXIT_FAILURE;
	}

	rc = listen(fd, MAX_BACKLOG);
	if(rc == -1){
		perror("listen() failed\n");
		return EXIT_FAILURE;
	}

	while(1){

		// buffer to hold message from client
		char* buf = calloc(MAX_BUFFER + 1, sizeof(char));
		// struct to hold client ip address
		struct sockaddr* client = calloc(1, sizeof(struct sockaddr));

		int len;

		// wait until a connection occurs
		int cliFd = accept(fd, client, &len);

		// send messages to client from stdin until client disconnects or EOF
		rc = interactWithClient(cliFd, client, buf);

		// clean up resources
		close(cliFd);
		free(buf);
		buf = NULL;
		free(client);
		client = NULL;

		// return code of 0 means end of file, terminate program
		if(rc == 0){
			close(fd);
			return 0;
		}


	}
		

}
