#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <netdb.h>
#include <math.h>

// max characters in an address
#define ADDRESS_LEN 64
// max characters in a site name
#define ID_LEN 64
// number of seconds to wait for select() call
#define TIMEOUT 15
// size of buffer for command line commands
#define CMD_SIZE 256
// max number of characters in xPos or yPos for MOVE command
#define INT_LEN 16


#define MAX_SIZE 64

// string literals for each command
#define MOVE_CMD "MOVE"
#define SEND_CMD "SENDDATA"
#define QUIT_CMD "QUIT"
#define WHERE_CMD "WHERE"


struct siteLst{
	char* id;
	struct siteLst* next;
	int xPos;
	int yPos;
};

struct hoplist{
	char* id;
	struct hoplist* next;
};

struct message{
	char* messageType;
	char* originID;
	char* nextID;
	char* destinationID;
	int hopLeng;
	struct hoplist* hoplst;
};

// takes in message m, fills in nextID from reachableSites and destinationID, and sends appropriate message to server
void sendMsg(struct message* m, struct siteLst* reachableSites){

	// closest to dest, ties alphabetically
	struct siteLst* closest;

	// pointer to walk through list of reachable sites
	struct siteLst* itr = reachableSites;
	while(itr != NULL){

	}

}

/*
Arg msg: message sent in, to be parsed and information put into struct message
	Must abide by format from specs, with a space(not NULL byte) at the end
Arg msgSize: Total size/length of message
Returns: struct message with info from msg
*/
struct message * parseMsg(char * msg, int msgSize){		
	char * cpy = calloc(strlen(msg)+1, sizeof(char));
	strcpy(cpy, msg);

	struct message* retMsg = calloc(1, sizeof(struct message));
	retMsg->messageType = calloc(ID_LEN, sizeof(char));
	strcpy(retMsg->messageType, strtok(cpy, " "));

	if (strcmp(retMsg->messageType, "DATAMESSAGE") == 0){

		retMsg->originID = calloc(ID_LEN, sizeof(char));
		strcpy(retMsg->originID, strtok(NULL, " "));

		retMsg->nextID = calloc(ID_LEN, sizeof(char));
		strcpy(retMsg->nextID, strtok(NULL, " "));

		retMsg->destinationID = calloc(ID_LEN, sizeof(char));
		strcpy(retMsg->destinationID, strtok(NULL, " "));

		retMsg->hopLeng = atoi(strtok(NULL, " "));

		retMsg->hoplst = calloc(1, sizeof(struct message));
		struct hoplist* iterator = retMsg->hoplst;

		for (unsigned int i = 0; i < retMsg->hopLeng; ++i){
			iterator->id = strtok(NULL, " ");
			if (i == retMsg->hopLeng-1){
				iterator->next = NULL;
			} else {
				iterator->next = calloc(1, sizeof(struct hoplist));
				iterator = iterator->next;
			}
		}

		free(cpy);
	} else {
		free(cpy);
	}
	return retMsg;
}

/*
Fnc: converts message struct into a string to be sent to the next "node"
Arg: struct message to be converted into a string to be sent elsewhere
Arg: thisID is the null-terminated string for the name of the sender (self)
Ret: string to be send, dynamically stored
*/
char* msgToStr(struct message* msg, char* thisID){
	int msg_size = strlen(msg->messageType) + strlen(msg->originID) + strlen(msg->nextID);
	msg_size += strlen(msg->destinationID) + (int)ceil(log10(msg->hopLeng))+6;
	struct hoplist* iterator = msg->hoplst;
	while (iterator != NULL){
		msg_size += strlen(iterator->id) + 1;
		iterator = iterator->next;
	}
	msg_size += strlen(thisID) + 1;

	char * str = calloc(msg_size+1, sizeof(char));
	strcat(str, msg->messageType);
	strcat(str, " ");
	strcat(str, msg->originID);
	strcat(str, " ");
	strcat(str, msg->nextID);
	strcat(str, " ");
	strcat(str, msg->destinationID);
	strcat(str, " ");
	char tmp[(int)ceil(log10(msg->hopLeng))+2];
	sprintf(tmp, "%d ", msg->hopLeng);
	strcat(str, tmp);
	iterator = msg->hoplst;
	while(iterator != NULL){
		strcat(str, iterator->id);
		strcat(str, " ");
		iterator = iterator->next;
	}
	iterator = NULL;
	strcat(str, thisID);
	strcat(str, " ");
	return str;
}

// deallocates all elements in this list
void freeLst(struct siteLst* lst){
	// if called on NULL, don't do anything
	if(!lst){
		return;
	}

	if(lst->next){
		freeLst(lst->next);
	}
	free(lst);
	return;
}

// sends new position message to server and waits for REACHABLE response, reinitializing lst
// returns pointer to new dynamically allocated linked list of siteLst
//
// lst: siteLst of all reachable sites.  If not null, deallocated
struct siteLst* updatePosition(struct siteLst* lst, int x, int y){

	// deallocate old list, if it exists
	if(lst){
		freeLst(lst);
	}

	// TODO:
	// make message
	// send message
	// receive response
	// initialize new list

}

// retrieve server address from name, and if able, connect to server
// returns 0 on success, else 1
int connectToServer(int sockfd, char* controlAddress, int controlPort){

	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	// not passive, returned sockaddr will be suitable for call to connect()
	hints.ai_flags = 0;

	if ((status = getaddrinfo(controlAddress, NULL, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error code: %s\n", gai_strerror(status));
		return 1;
	}

	status = 1;


	// connect to server at given address by name
	struct addrinfo * iterator = servinfo;
	// iterate over all addresses found for given name
	while (iterator != NULL){

		int family = iterator->ai_family;
		// IPv4
		if(family == AF_INET){
			struct sockaddr_in* info = (struct sockaddr_in*)iterator->ai_addr;
			// check port is correct
			if(info->sin_port == controlPort){
				if(connect(sockfd, (struct sockaddr*)info, sizeof(info)) != 0){
					perror("ERROR: connect() failed\n");
					break;
				}
				// successful connect, return value will be 0
				status = 0;
				break;
			}

		} 
		// IPv6
		else if(family == AF_INET6){
			struct sockaddr_in6* info = (struct sockaddr_in6*)iterator->ai_addr;	
			// check port is correct
			if(info->sin6_port == controlPort){

				if(connect(sockfd, (struct sockaddr*)info, sizeof(info)) != 0){
					perror("ERROR: connect() failed\n");
					break;
				}
				// successful connect, return value will be 0
				status = 0;
				break;
			}
		}
		else{
			printf("Unrecognized family\n");
		}

		iterator = iterator->ai_next;
	}

	freeaddrinfo(servinfo);

	return status;

}

// takes in command from stdin and executes user command

/* Commands include:

MOVE [NewXPosition] [NewYPosition]
SENDDATA [DestinationID]
QUIT
WHERE [SensorID/BaseID]

*/
int interactWithConsole(struct siteLst* reachableSites){
	// buffer to hold command
	char buf[CMD_SIZE];

	// read next line from command line into buf
	fgets(buf, CMD_SIZE, stdin);

	char command[CMD_SIZE];

	strcpy(command, strtok(buf, " "));	

	// MOVE [NewXPosition] [NewYPosition]
	if(strcmp(command, MOVE_CMD) == 0){
		// buffer to hold new coordinates
		char pos[INT_LEN];
		// 0 out buffer and copy xposition
		memset(pos, '\0', INT_LEN);
		strcpy(pos, strtok(buf, " "));
		int xPos = atoi(pos);
		// 0 out buffer and copy yposition
		memset(pos, '\0', INT_LEN);
		strcpy(pos, strtok(buf, " \0"));
		int yPos = atoi(pos);

		// update position and send message to server
		updatePosition(reachableSites, xPos, yPos);

		return 0;

	}
	// SENDDATA [DestinationID]
	else if(strcmp(command, SEND_CMD) == 0){
		
	}
	// QUIT
	else if(strcmp(command, QUIT_CMD) == 0){
		
	}
	// WHERE [SensorID/BaseID]
	else if(strcmp(command, WHERE_CMD) == 0){
		
	}
/*
	char * cpy = calloc(strlen(msg)+1, sizeof(char));
	strcpy(cpy, msg);

	struct message* retMsg = calloc(1, sizeof(struct message));
	retMsg->messageType = calloc(MAX_SIZE, sizeof(char));
	strcpy(retMsg->messageType, strtok(cpy, " "));

	if (strcmp(retMsg->messageType, "DATAMESSAGE") == 0){

		retMsg->originID = calloc(MAX_SIZE, sizeof(char));
		strcpy(retMsg->originID, strtok(NULL, " "));

		retMsg->nextID = calloc(MAX_SIZE, sizeof(char));
		strcpy(retMsg->nextID, strtok(NULL, " "));

		retMsg->destinationID = calloc(MAX_SIZE, sizeof(char));
		strcpy(retMsg->destinationID, strtok(NULL, " "));

		retMsg->hopLeng = atoi(strtok(NULL, " "));

		retMsg->hoplst = calloc(1, sizeof(struct message));
		struct hoplist* iterator = retMsg->hoplst;

		for (unsigned int i = 0; i < retMsg->hopLeng; ++i){
			iterator->id = strtok(NULL, " ");
			if (i == retMsg->hopLeng-1){
				iterator->next = NULL;
			} else {
				iterator->next = calloc(1, sizeof(struct hoplist));
				iterator = iterator->next;
			}
		}

		free(cpy);
	} else {
		free(cpy);
	}
	return retMsg;
	*/



}

int main(int argc, char * argv[]) {

	// check correct number of arguments
	if(argc != 7){
		printf("Incorrect arguments, usage: ");
		printf("./client.out [control address] [control port] [SensorID] [SensorRange] [InitalXPosition] [InitialYPosition]\n");

		return 1;
	}

	// initialize variables from command line arguments

	// address or hostname of server
	char* controlAddress = calloc(ADDRESS_LEN + 1, sizeof(char));
	strncpy(controlAddress, argv[1], ADDRESS_LEN);
	
	// port to reach server
	int controlPort = atoi(argv[2]);
	
	// name of this sensor site
	char* sensorID = calloc(ID_LEN + 1, sizeof(char));
	strncpy(sensorID, argv[3], ID_LEN);
	
	// can communicate only to other sites in this range
	int SensorRange = atoi(argv[4]);
	
	// x and y coordinates
	int xPos = atoi(argv[5]);
	int yPos = atoi(argv[6]);


	// make socket, try to connect

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// resolves server name and connects socket to server
	connectToServer(sockfd, controlAddress, controlPort);


	// initialize list of reachable sites to empty
	struct siteLst* reachableSites = NULL;

	// sends initial updatePosition message and initializes list of reachable sites
	updatePosition(reachableSites, xPos, yPos);

	// ready to loop?

	// structure to hold a set of file descriptors to watch
	fd_set rfds;

	// file descriptor for stdin, could change based on piping or other output shenanigans
	int standardInput = fileno(stdin);

	while(1){


		// set rfds to include no file descriptors
		FD_ZERO(&rfds);

		// add stdin to fdset
		FD_SET(standardInput, &rfds);

		// add socket for server to fdset
		FD_SET(sockfd, &rfds);

		// structure to specify TIMEOUT second timeout on select() call
		struct timeval timeout;

		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = 0;

		// wait for activity on listening socket, or any active client
		int retval = select(sockfd > standardInput ? sockfd : standardInput, &rfds, NULL, NULL, &timeout);

		if(retval == 0){
			printf("No Activity\n");
			continue;
			//return 0;
		}
		else if(retval == -1){
			perror("ERROR Select() failed\n");
			return EXIT_FAILURE;
		}

		if(FD_ISSET(standardInput, &rfds)){
			// TODO:
			interactWithConsole(reachableSites);
		}

		if(FD_ISSET(sockfd, &rfds)){
			// TODO:
			// recvMsg()
		}

	}


	return 0;
}

/*

select() to read from stdin and any tcp connections

sendMsg()

recvMsg()

interactWithConsole() // interpreting commands
*/