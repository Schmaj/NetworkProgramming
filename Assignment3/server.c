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
#include <pthread.h>

// max number of characters in filename
#define MAX_FILE 64
// number of tcp connection requests allowed in backlog
#define BACKLOG 10
// number of seconds to block on select() call
#define TIMEOUT 15
// maximum number of clients allowed to connect
#define MAX_CLIENTS 16
// value to signify no client has connected at this index of the array
#define NO_CLIENT -1

struct message;
struct hoplist;
char* msgToStr(struct message* msg, char* thisID);
struct baseStation;
struct siteLst;
void updateSiteLst(char* sensorID, int xPosition, int yPosition);

struct siteLst* globalSiteList;


struct message{
	char* messageType;
	char* originID;
	char* nextID;
	char* destinationID;
	int hopLeng;
	struct hoplist* hoplst;
};

struct hoplist{
	char* id;
	struct hoplist* next;
};

// new siteLst created for each baseStation - list of sites (clients will be added when they enter range)
struct siteLst{
	char* id;
	struct siteLst* next;
	int xPos;
	int yPos;
};

struct baseStation{
	int xPos;
	int yPos;
	char* id;
	struct siteLst* connectedLst;
};

// initializes global list of base stations from base station file
void initializeBaseStations(FILE* baseStationFile){
	if (baseStationFile == NULL){
		perror("initializeBaseStations, baseStationFile");
		exit(EXIT_FAILURE);
	}
	char* line = NULL;
	int read;
	size_t len = 0;
	while((read = getline(&line, &len, baseStationFile)) != -1){

	}
	fclose(baseStationFile);
	if (line) free(line);
}

/*
Fnc: converts message struct into a string to be sent to the next "node"
Arg: struct message to be converted into a string to be sent elsewhere
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


/*
Ret: void
Arg: msg is the message received. if msg->messageType == "Update Position", 
		update globalSiteList
*/
void updateSiteLst(char* sensorID, int xPosition, int yPosition){ // call that for everybody
	struct siteLst* iterator = globalSiteList;
	while(iterator != NULL){
		iterator = iterator->next;
	}
	iterator->next = calloc(1, sizeof(struct siteLst));
	iterator = iterator->next;
	iterator->id = calloc(strlen(sensorID)+1, sizeof(char));
	strcpy(iterator->id, sensorID);
	iterator->xPos = xPosition;
	iterator->yPos = yPosition;
	iterator->next = NULL;		//just to be sure lol
	iterator = NULL;
	return;
}

int interactWithConsole(){


	return 0;
}

int main(int argc, char * argv[]) {
	
	// check for correct number of arguments
	if(argc != 3){
		printf("incorrect arguments\nUsage: ./server.out [control port] [base station file]");
		return 0;
	}

	// read in port from arguments
	int port = atoi(argv[1]);

	// copy filename from arguments
	char* fileName = calloc(MAX_FILE, sizeof(char));
	strcpy(fileName, argv[2]);

	// open base station file for reading
	FILE* baseStationFile = fopen(fileName, "r");

	free(fileName);
	fileName = NULL;

	initializeBaseStations(baseStationFile);

	// create socket
	int listenerFd = socket(AF_INET, SOCK_STREAM, 0);

	if(listenerFd < 0){
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
	// initialize to given port
	server.sin_port = htons(port);

	// size of struct for bind()
	int len = sizeof(struct sockaddr_in);

	// bind listening server to address 
	if(bind(listenerFd, (struct sockaddr*)&server, len) != 0){
		perror("bind() failed\n");
		return EXIT_FAILURE;
	}

	if(listen(listenerFd, BACKLOG)){
		perror("ERROR: listen() failed\n");
		return EXIT_FAILURE;
	}


	// structure to hold a set of file descriptors to watch
	fd_set rfds;

	// file descriptor for stdin, could change based on piping or other output shenanigans
	int standardInput = fileno(stdin);

	// select over stdin (for quit), listener (for new clients), tcp_comm sockets (for datamessages), and start new thread to handle message when message comes in
	while(1){

		int maxFd = standardInput > listenerFd ? standardInput : listenerFd;

		// set rfds to include no file descriptors
		FD_ZERO(&rfds);

		// add stdin to fdset
		FD_SET(standardInput, &rfds);

		// add socket for server to fdset
		FD_SET(listenerFd, &rfds);

		// TODO: loop over connected clients and add to fdset

		// structure to specify TIMEOUT second timeout on select() call
		struct timeval timeout;

		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = 0;

		// wait for activity on listening socket, or any active client
		int retval = select(maxFd, &rfds, NULL, NULL, &timeout);

		if(retval == 0){
			printf("No Activity\n");
			continue;
			//return 0;
		}
		else if(retval == -1){
			perror("ERROR Select() failed\n");
			return EXIT_FAILURE;
		}

		// received command message on stdin
		if(FD_ISSET(standardInput, &rfds)){
			
			int quit = interactWithConsole();

			if(quit == 1){
				// TODO: clean up resources, wait for threads to finish, kill
			}
		}
		// new client connecting
		if(FD_ISSET(listenerFd, &rfds)){
			// TODO: accept connection
		}
		/*
		for(int n = 0; n < numClients; n++){
			if(FD_ISSET(clients[n], &rfds)){
				// TODO: make new thread to deal with message
			}
		}
		*/
	}


	return 0;
}
/*
baseStationToBaseStation()

recvMsgFromClient()

serverToClient()

interactWithConsole() // getting command from user




*/