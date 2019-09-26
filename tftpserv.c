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

int parse_packet(char* buffer, int size){
	if (size <= 0){
		return -1;
	}
	unsigned short opcode = ntohs((buffer[0] << 8)|buffer[1]);

}


int main(int argc, char* argv[]){

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
		int rc = bind(fd, (sockaddr*)server, sizeof(server));
		if(rc < 0 ){
			perror("bind() failed\n");
			return EXIT_FAILURE;
		}

		// send to child fd, buffer, sockaddr of client





	}

}
