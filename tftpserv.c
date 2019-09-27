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

#define MAX_PACKET 512

int children = 0;

void childFunction(unsigned int fd, char* buffer, sockaddr* addr){
	unsigned short int opcode = ntohs((buffer[0] << 8) | buffer[1]);
	char* Filename[MAX_PACKET];
	char* Mode[MAX_PACKET];
	strcpy(Filename, &buffer[2]);
	if (opcode == 1){ // READ
		unsigned int file_d = open(Filename, O_RDONLY);
		if (file_d == -1){	//ERROR
			perror("childFuntion, Read, Open");
		}
		
	} else if (opcode == 2) { //WRITE
		unsigned int file_d = open(Filename, O_WRONLY);
		if (file_d == -1){ //ERROR
			perror("childFunction, Write, Open");
		}

	}
	return;
}

void terminate(){
	printf("Exiting program\n");
	int stat;
	printf("%d children remaining\n", children);
	// loop until all children have been collected
	while(children > 0){
		wait(&stat);
		children--;
		printf("caught child\n");
	}
	exit(0);
}

void sig_interrupt(int signo){
	// do the things

	printf("Caught interrupt\n");

	terminate();
}

int main(int argc, char* argv[]){

	// struct to hold desired sigaction
	struct sigaction sigintResponse;
	// specify function to be called as handler
	sigintResponse.sa_handler = &sig_interrupt;
	// initialize sa_mask to an empty set
	sigemptyset(&(sigintResponse.sa_mask));
	// no flags
	sigintResponse.sa_flags = 0;

	// sets new action for receiving interrupt signal
	int rc = sigaction(SIGINT, &sigintResponse, NULL);
	if(rc < 0){
		perror("ERROR with sigaction\n");
		return EXIT_FAILURE;
	}

	// set the response to ingore signal, will be applied to children when created so they may finish
	// normally when a signal interrupt is sent to the parent
	sigintResponse.sa_handler = SIG_IGN;

	// check number of arguments
	if(argc < 2){
		printf("Insufficient arguemnts\n");
		printf("Usage: ./serv.out portMin portMax\n");
	}

	// initialize port range from arguments
	int portMin = atoi(argv[1]);
	int portMax = atoi(argv[2]);

	// struct to hold ip address and port of udp server waiting for client
	struct sockaddr_in server;

	// use IPv4
	server.sin_family = AF_INET;
	// allow any ip address
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	// initialize to min port in port range
	server.sin_port = htons(portMin);

	while(1){

		// file descriptor referring to a socket listening for the next UDP client
		// creates a socket, IPv4, UDP, default protocol
		int fd = socket(AF_INET, SOCK_DGRAM, 0);
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

		// send to child fd, buffer, sockaddr of client

		// buffer to hold message from client
		char* buf = calloc(MAX_PACKET, sizeof(char));
		// struct to hold client ip address
		struct sockaddr* client = calloc(1, sizeof(struct sockaddr));

		int len;

		// wait until a request is received and store number of bytes read
		int readBytes = recvfrom(fd, buf, MAX_PACKET, 0, client, (socklen_t*)&len);

		if(readBytes == -1){
			perror("recvfrom() failed\n");
			return EXIT_FAILURE;
		}

		// create child process to handle communication
		int pid = fork();
		// error
		if(pid < 0){
			perror("fork() failed\n");
			return EXIT_FAILURE;
		}
		// child
		else if(pid == 0){
			// tells children to ignore signal interupt, signal interrupt will be
			// meant for parent alone
			int rc = sigaction(SIGINT, &sigintResponse, NULL);
			if(rc < 0){
				perror("ERROR with sigaction\n");
				return EXIT_FAILURE;
			}
			childFunction(fd, buf, client);

			// close finished socket descriptor
			close(fd);

			// deallocate memory
			free(buf);
			buf = NULL;
			free(client);
			client = NULL;
		}

		// increment number of children
		children++;

		// close file descriptor to previous socket in parent
		close(fd);

		// increment port number
		portMin++;
		server.sin_port = htons(portMin);


	}

}
