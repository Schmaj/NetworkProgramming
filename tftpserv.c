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

	while(1){

		// file descriptor referring to a socket listening for the next UDP client
		//int fd = socket

	}

}
