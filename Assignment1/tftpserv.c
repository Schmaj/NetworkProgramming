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

// flag to show whether SIGINT has been received and process
// should terminate
// volatile memory because its value is changed due to a signal handler, and a while
// loop depends on its value, which will not change during normal execution of the loop,
// want to avoid compiler optimizations not rechecking its value, causing an infinite loop
volatile int term = 0;

// number of child processes spawned
int children = 0;


// cleans up child resources and exits program, called by parent
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

// function to be called when interrupt signal is received by child,
// indicating that child should allow itself to exit once current request is
// dealt with
// child will not continue to loop waiting for new requests when term is set to 1
void sig_int_handler_child(int signo){
	printf("Kill signal received\n");
	term = 1;
	return;
}

// function does not do anything, just allows signal to interrupt a blocking read without terminating the process
void resendDataAlarm(int signo){
	return;
}

// function for most functionality of the program
// takes in information of where messages should be sent
// parses for opcode and handles RRQ and WRQ separately
// opens file to read or write to, and sends messages in order to accomplish that
//
// RRQ opens a local file from which to read and sends the 512 byte chunk of data to
// the desired address every second until the next ack message is received
//
// WRQ opens a local file to write to, and receives 512 byte chunks of data that are then
// written to this file
// after each data block is received, an ack is sent
//
// for either functionality, any messages that do not match what was expected
// (i.e. wrong opcode or wrong blocknumber) are ignored, as they may be due to 
// latency issues
//
// this function, after successfully completing its request, loops waiting for another
// request until a signal interrupt is received, then returns for the last dynamically allocated
// memory to be freed, and the process to exit gracefully
void childFunction(unsigned int fd, char* buffer, struct sockaddr* addr, socklen_t cli_len){

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

	// variable to represent that the connection timed out, or process would which to return for other
	// reason.  However, the process must continue to loop waiting for new connections, so when this variable is
	// nonzero, the process will break to its outermost loop
	int fullBreak = 0;

	// simple variable to change behavior of first iteration through the following loop
	int firstRun = 0;

	// loop until termination signal
	while(term == 0){

		// reset fullBreak to 0, just in case
		fullBreak = 0;

		// subsequent iterations through loop
		if(firstRun != 0){


			// loop in case of error
			while(1){
				// zero out buffer and client address
				bzero(buffer, MAX_PACKET);
				bzero(addr, cli_len);

				cli_len = sizeof(struct sockaddr_in);

				// set 1 second alarm in case of weird race conditions with term signal, don't
				// block forever on read
				alarm(1);

				// stores number of bytes read
				int br = recvfrom(fd, buffer, MAX_PACKET, 0, addr, &cli_len);

				// cancel alarm if it did not go off
				alarm(0);

				// error
				if(br == -1){
					//printf("recvfrom() failed child, errno %d\n", errno);

					// if error was interrupt, term may be set and should be checked to not
					// block indefinitely
					if(term == 1){
						return;
					}

					// try to read bytes again
					continue;

				}

				// if bytes correctly received, break out of loop and handle request
				break;

			}


		}

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

				// send error packet
				// error code 1 is file not found
				char err[19];
				bzero(err, 19);
				// 5 is error code
				((short*)err)[0] = htons(5);
				// 1 is code for file not found
				((short*)err)[1] = htons(1);

				// copy message into err
				memcpy(&err[4], "File not Found\0", 15);

				int size = sendto(fd, err, 19, 0, addr, sizeof(struct sockaddr_in));

				// go back to main loop
				firstRun = 1;
				continue;
			}

			printf("OPEN_SUCCESS\n");


			// send first data packet, wait 1 second to resend anything, 10 seconds to timeout
			// error if packet from wrong tid, send error packet and ignore

			// data packets of the form 2 byte opcode, 2 byte block number, n byte data
			// ACk packets of the form 2 byte opcode, 2 byte block number
			

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
							
							// set fullBreak to 1 to break out of all loops but the outermost loop
							fullBreak = 1;
							break;
						}
						continue;
					}
					// if no interrupt, break and continue
					break;

				}

				// if fullBreak nonzero, break to outermost loop (response was alread freed)
				if(fullBreak != 0){
					break;
				}

				// seen acknowledgement of data, move onto next block
				blockNumber++;

				free(response);
				response = NULL;



			}

			// if fullBreak nonzero, in outermost loop, reset fullBreak to 0
			if(fullBreak != 0){
				fullBreak = 0;
			}

			// close file being read
			close(file_d);

			

		} else if (opcode == 2) { //WRITE
			//Open file to write
			unsigned int file_d = open(Filename, O_CREAT | O_WRONLY | O_TRUNC, 0777);
			if (file_d == -1){ //ERROR
				perror("childFunction, Write, Open");
				// reset to main loop
				firstRun = 1;
				continue;
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
					continue;
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

								// set fullBreak to 1 and break to outermost loop
								fullBreak = 1;
								break;
							}
							continue;
						}
						// if no interrupt, break and continue
						break;
					}
					// if fullBreak nonzero, break to outermost loop
					if(fullBreak != 0){
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
					write(file_d, data, size-4);
					((short*)ack)[0] = htons(4);
					((short*)ack)[1] = htons(blockcount);
					sendto(fd, ack, 4, 0, addr, sizeof(struct sockaddr_in));
					close(file_d);
					// set fullBreak to 1 and break to outermost loop
					fullBreak = 1;
					break;
				}	
				write(file_d, data, size-4);
				((short*)ack)[0] = htons(4);
				((short*)ack)[1] = htons(blockcount);
				sendto(fd, ack, 4, 0, addr, sizeof(struct sockaddr_in));
			}

			// in outermost loop, reset fullbreak
			if(fullBreak != 0){
				fullBreak = 0;
				firstRun = 1;
			}

		}

		firstRun = 1;

	}
	// finally return, no more requests
	return;
}

// parent process will terminate and wait for all children upon receiving
// a signal interrupt
void sig_interrupt(int signo){

	printf("Caught interrupt\n");

	terminate();
}

// main function, sets various signal behaviors and creates sockets
// UDP sockets are created on ports starting from portMin, and incrementing
// every time a request is received on the latest port, forking, and allowing
// a child process to handle that request
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
	sigintResponse.sa_handler = &sig_int_handler_child;

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

		// allow any ip address
		server.sin_addr.s_addr = htonl(INADDR_ANY);

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

		unsigned int len = sizeof(struct sockaddr_in);

		printf("PRE-READ\n");

		// wait until a request is received and store number of bytes read
		int readBytes = recvfrom(fd, buf, MAX_PACKET, 0, (struct sockaddr*)client, (socklen_t*)&len);

		printf("READ\n");

		printf("client is at port %d\n", ntohs((*client).sin_port));


		if(readBytes == -1){
			perror("recvfrom() failed\n");
			printf("recvfrom() failed: errno %d\n", errno);
			// clean up resources and try again
			if(buf != NULL)
				free(buf);
			if(client != NULL)
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

			// tells children what to do in reaction to signal interrupt:
			// will set variable to allow child to exit on interrupt
			int rc = sigaction(SIGINT, &sigintResponse, NULL);
			if(rc < 0){
				perror("ERROR with sigaction\n");
				return EXIT_FAILURE;
			}

			childFunction(fd, buf, (struct sockaddr*)client, len);

			// close finished socket descriptor
			close(fd);

			// deallocate memory
			if(buf != NULL)
				free(buf);
			buf = NULL;
			if(client != NULL)
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
