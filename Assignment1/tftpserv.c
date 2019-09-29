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

#define MAX_PACKET 516

int children = 0;


// function does not do anything, just allows signal to interrupt a blocking read without terminating the process
void resendDataAlarm(int signo){
	return;
}

void childFunction(unsigned int fd, char* buffer, struct sockaddr* addr, socklen_t cli_len){
	//unsigned short int opcode = ntohs((buffer[0] << 8) | buffer[1]);
	unsigned short int opcode = ntohs((*(unsigned short int*)buffer));
	char Filename[MAX_PACKET];

	strcpy(Filename, &buffer[2]);

	printf("In child, filename %s, opcode %u\n", Filename, opcode);


	if (opcode == 1){ // READ
		// open file requested by client
		unsigned int file_d = open(Filename, O_RDWR);
		if (file_d == -1){	//ERROR
			perror("childFuntion, Read, Open");
		}

		printf("OPEN_SUCCESS\n");


		// send first data packet, wait 1 second to resend anything, 10 seconds to timeout
		// error if packet from wrong tid, send error packet and ignore

		// data packets of the form 2 byte opcode, 2 byte block number, n byte data
		// ACk packets of the form 2 byte opcode, 2 byte block number
		
		// set desired SIGALRM behavior for resending data
		// struct to hold desired sigaction
		struct sigaction response;
		// specify function to be called as handler
		response.sa_handler = &resendDataAlarm;
		// initialize sa_mask to an empty set
		sigemptyset(&(response.sa_mask));
		// no flags
		response.sa_flags = 0; 

		// sets new action for receiving alarm signal
		int rc = sigaction(SIGALRM, &response, NULL);
		if(rc < 0){
			printf("ERROR with sigaction\n");
			return;
		}

		// send data, set alarms, wait
		// initialize readBytes to value for data to continue to be transferred
		int readBytes = MAX_PACKET - 4;

		// keeps track of current block number
		unsigned int blockNumber = 1;
		// opcode for data block
		short dataCode = 3;

		while(readBytes == MAX_PACKET - 4){
			// casts buffer to short pointer to set first two bytes and second 2 bytes as opcode and blocknumber
			// respectively, in network byte order
			((short*)buffer)[0] = htons(dataCode);
			((short*)buffer)[1] = htons(blockNumber);

			// buffer to hold acknowledgement
			char* response = calloc(MAX_PACKET, sizeof(char));



			// read the next 512 bytes of the file into the buffer
			readBytes = read(file_d, buffer + 4, MAX_PACKET - 4);

			// counts number of times we resend messages
			int n = 0;

			while(1){

				printf("PRE-SEND\n");
				// send current block of data
				sendto(fd, buffer, readBytes + 4, 0, addr, cli_len);
				printf("POST-SEND\n");

				// resend after 1 second
				alarm(1);

				// wait for response, will be interrupted after 1 second
				int bytes = recvfrom(fd, response, MAX_PACKET, 0, NULL, NULL);

				alarm(0);
				// if recvfrom was interrupted, or the message received had the wrong block number, resend
				if( (bytes == -1 && errno == EINTR) || ntohs((*(((unsigned short int*)response)+1))) != blockNumber){
					// increment number of times we have sent message
					n++;
					printf("No response, resending block %d\n", blockNumber);
					// timeout after 10 seconds
					if(n == 10){
						printf("Timeout occurred after 10 seconds\n");
						// clean up resources and terminate
						free(response);
						response = NULL;
						close(file_d);
						return;
					}
					continue;
				}
				// if no interrupt, break and continue
				break;

			}

			// seen acknowledgement of data, move onto next block
			blockNumber++;

			free(response);
			response = NULL;



		}

		// close file being read
		close(file_d);

	} else if (opcode == 2) { //WRITE
		//Open file to write
		unsigned int file_d = open(Filename, O_CREAT | O_WRONLY);
		if (file_d == -1){ //ERROR
			perror("childFunction, Write, Open");
		}
		//Zero buffer and send an ACK back
		bzero(buffer, MAX_PACKET);
		char ack[4];
		bzero(ack, 4);
		((short*)ack)[0] = htons(4);
		int size = sendto(fd, ack, 4, 0, addr, sizeof(struct sockaddr_in));
		if (size <= 0){
			perror("childFunction, Write, AckSend");
			return;
		}

		struct sigaction response;
		// specify function to be called as handler
		response.sa_handler = &resendDataAlarm;
		// initialize sa_mask to an empty set
		sigemptyset(&(response.sa_mask));
		// no flags
		response.sa_flags = 0; 

		// sets new action for receiving alarm signal
		int rc = sigaction(SIGALRM, &response, NULL);
		if(rc < 0){
			printf("ERROR with sigaction\n");
			return;
		}

		unsigned short int n = 0;
		unsigned int blockcount = 0;
		unsigned int blocknum;
		while(1){
			bzero(buffer, MAX_PACKET);
			//wait for data packet
			size = recvfrom(fd, buffer, MAX_PACKET, 0, NULL, NULL);

			//Error if recvfrom failed
			if (size <= 0){
				perror("childFunction, Loop, recvfrom");
				return;
			}

			//bitmask to get opcode
			opcode = ntohs((*(unsigned short int*)buffer));
			if (opcode != 3){ //ERROR
				perror("childFunction, Loop, != DATA");
				continue;
			}

			//bitmask to get block number, resend ack if wrong block number
			blocknum = ntohs((*(((unsigned short int*)buffer)+1)));
			printf("Received block %u\n", blocknum);
			if (blocknum != blockcount + 1){ //Wrong Order
				printf("ENTER IF\n");
				while(1){
					printf("NEW ITERATION\n");
					((short*)ack)[0] = htons(4);
					((short*)ack)[1] = htons(blockcount);
					sendto(fd, ack, 4, 0, addr, sizeof(struct sockaddr_in));

					// resend after 1 second
					alarm(1);

					bzero(buffer, MAX_PACKET);
					// wait for response, will be interrupted after 1 second
					size = recvfrom(fd, buffer, MAX_PACKET, 0, NULL, NULL);

					alarm(0);
					// if recvfrom was interrupted, or the message received had the wrong block number, resend
					if( (size == -1 && errno == EINTR) || ntohs((*(unsigned short int*)buffer+1)) != blockcount+1){
						// increment number of times we have sent message
						n++;
						// timeout after 10 seconds
						if(n == 10){
							// clean up resources and terminate
							close(file_d);
							return;
						}
						continue;
					}
					// if no interrupt, break and continue
					break;
				}
			}

			//increment block counter, reset resend count
			blockcount++;
			n = 0;

			//zero buffer, write data, send ACK
			char data[MAX_PACKET];
			bzero(data, MAX_PACKET);
			memcpy(data, &buffer[4], MAX_PACKET-4);
			data[MAX_PACKET] = '\0';
			if (size != MAX_PACKET){ //END OF TRANSMISSION
				write(file_d, data, size-5);
				((short*)ack)[0] = htons(4);
				((short*)ack)[1] = htons(blockcount);
				sendto(fd, ack, 4, 0, addr, sizeof(struct sockaddr_in));
				close(file_d);
				return;
			}	
			write(file_d, data, size-4);
			((short*)ack)[0] = htons(4);
			((short*)ack)[1] = htons(blockcount);
			sendto(fd, ack, 4, 0, addr, sizeof(struct sockaddr_in));
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
		printf("Insufficient arguments\n");
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
		struct sockaddr_in* client = calloc(1, sizeof(struct sockaddr_in));

		int len;

		printf("PRE-READ\n");

		// wait until a request is received and store number of bytes read
		int readBytes = recvfrom(fd, buf, MAX_PACKET, 0, (struct sockaddr*)client, (socklen_t*)&len);

		printf("READ\n");

		printf("client is at port %d\n", ntohs((*client).sin_port));


		if(readBytes == -1){
			perror("recvfrom() failed\n");
			// clean up resources and try again
			free(buf);
			free(client);
			close(fd);
			continue;
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

			childFunction(fd, buf, (struct sockaddr*)client, len);

			// close finished socket descriptor
			close(fd);

			// deallocate memory
			free(buf);
			buf = NULL;
			free(client);
			client = NULL;

			return 0;
		}

		// increment number of children
		children++;

		// close file descriptor to previous socket in parent
		close(fd);

		free(buf);
		buf = NULL;

		// increment port number
		portMin++;
		server.sin_port = htons(portMin);

		if(portMin > portMax){
			break;
		}


	}

	int stat;
	printf("%d children remaining\n", children);
	// loop until all children have been collected
	while(children > 0){
		wait(&stat);
		children--;
		printf("caught child\n");
	}

}
