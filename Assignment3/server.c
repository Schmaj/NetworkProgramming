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
// value to signify this client does not have a thread currently running
#define NO_THREAD 0

#define CMD_SIZE 256
// max number of characters in xPos or yPos for MOVE command
#define INT_LEN 16
//max number of bytes to be read in
#define MAX_REACHABLE 32

// maximum expected number of hops in a hop list
#define MAX_HOP 16

#define MAX_SIZE 64


#define DATA_MSG "DATAMESSAGE"
#define WHERE_MSG "WHERE"
#define UPDATE_MSG "UPDATEPOSITION"
#define SEND_CMD "SENDDATA"
#define CONTROL "CONTROL"

// max characters in a site name
#define ID_LEN 64

struct message;
struct hoplist;
char* msgToStr(struct message* msg, char* thisID);
struct baseStation;
struct siteLst;
void updateSiteLst(char* sensorID, int xPosition, int yPosition);

struct siteLst* globalSiteList;
struct baseStation* globalBaseStationList; //10


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
	uint isBaseStation;
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

struct client{
	int fd;
	struct siteLst* site;
	pthread_t tid;
};

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

// initializes global list of base stations from base station file
void initializeBaseStations(FILE* baseStationFile){
	if (baseStationFile == NULL){
		perror("initializeBaseStations, baseStationFile");
		exit(EXIT_FAILURE);
	}
	char* line = NULL;
	int read;
	size_t len = 0;
	uint index = 0;
	globalBaseStationList = calloc(10, sizeof(struct baseStation));
	
	while((read = getline(&line, &len, baseStationFile)) != -1){
		globalBaseStationList[index].id = calloc(ID_LEN+1, sizeof(char));
		strncpy(globalBaseStationList[index].id, strtok(line, " "), ID_LEN);

		globalBaseStationList[index].xPos = atoi(strtok(NULL, " "));
		globalBaseStationList[index].yPos = atoi(strtok(NULL, " "));

		uint degree = atoi(strtok(NULL, " "));
		globalBaseStationList[index].connectedLst = calloc(1, sizeof(struct siteLst));
		struct siteLst* iterator = globalBaseStationList[index].connectedLst;
		for (uint i = 0; i < degree; ++i){
			iterator->isBaseStation = 1;
			iterator->id = calloc(ID_LEN+1, sizeof(char));
			if (i == degree-1){
				strncpy(iterator->id, strtok(NULL, "\n"), ID_LEN);
				iterator->next = NULL;
			} else {
				strncpy(iterator->id, strtok(NULL, " "), ID_LEN);
				iterator->next = calloc(1, sizeof(struct siteLst));
				iterator = iterator->next;
			}			
		}
		index++;
	}

	for (uint i = 0; i < 10; ++i){
		if (globalBaseStationList[i].id == NULL) continue;
		for (uint j = 0; j < 10; ++j){
			if (i == j) continue;
			struct siteLst* siteItr = globalBaseStationList[j].connectedLst;
			while(siteItr){
				if (strcmp(globalBaseStationList[i].id, siteItr->id) == 0){
					siteItr->xPos = globalBaseStationList[i].xPos;
					siteItr->yPos = globalBaseStationList[i].yPos;
					break;
				}
				siteItr = siteItr->next;
			}
		}
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

int interactWithConsole(/*don't know what we need yet*/){
	// buffer to hold command
	char buf[CMD_SIZE];

	// read next line from command line into buf
	fgets(buf, CMD_SIZE, stdin);

	char command[CMD_SIZE];

	strcpy(command, strtok(buf, " "));
	// SENDDATA [DestinationID]
	if(strcmp(command, SEND_CMD) == 0){

		struct message* m = calloc(1, sizeof(struct message));

		// initialize messageType
		m->messageType = calloc(ID_LEN, sizeof(char));
		strcpy(m->messageType, DATA_MSG);

		m->originID = calloc(ID_LEN, sizeof(char));
		strcpy(m->originID, strtok(NULL, " "));

		// nextID initially null, will be determined 
		m->nextID = NULL;

		// destination ID intialized to destination given by user
		m->destinationID = calloc(ID_LEN, sizeof(char));
		strcpy(m->destinationID, strtok(buf, "\n"));

		// hopleng and hoplist initially empty, will be updated to include self in sendDataMsg()
		m->hopLeng = 0;
		m->hoplst = NULL;

		struct siteLst* knownLocations;
		sendDataMsg(sensorID, sockfd, m, reachableSites, knownLocations);

		// TODO free message
		freeMsg(m);

		return 0;


	}
	// QUIT
	else if(strcmp(command, QUIT_CMD) == 0){
		// return value of 1 signals quit, all real cleanup will happen in main
		return 1;
		
	}

	return 0;
}

/*

// receive a message from socket
int recvMsg(int sockfd, char* myID, struct siteLst* reachableSites, struct siteLst* knownLocations){

	// estimated max size of message
	// length of name (plus 1 character for space) multiplied by the max number of names (full hop list + originalSiteId
	// + destinationSiteID) + message type length (MAX_SIZE) + integer (hoplength)
	int msgSize = (ID_LEN + 1) * (MAX_HOP + 2) + MAX_SIZE + INT_LEN;

	// buffer to hold message from server
	char* buf = calloc(msgSize, sizeof(char));

	int bytes = read(sockfd, buf, msgSize);

	struct message* m = parseMsg(buf, bytes);

	// if this site is the destination
	if(strcmp(m->destinationID, myID) == 0){
		// print that message was properly received
		printf("%s: Message from %s to %s successfully received\n", myID, m->originID, myID);
		return 0;
	}

	sendDataMsg(myID, sockfd, m, reachableSites, knownLocations);

	return 0;
}
*/

void giveToBaseStation(char* baseID, struct message* m){
	// if this site is the destination
	if(strcmp(m->destinationID, baseID) == 0){
		// print that message was properly received
		printf("%s: Message from %s to %s successfully received\n", baseID, m->originID, baseID);
	}
}

void* handleMessage(void* args){
	struct client* cli = (struct client*)args;

	// estimated max size of message
	// length of name (plus 1 character for space) multiplied by the max number of names (full hop list + originalSiteId
	// + destinationSiteID) + message type length (MAX_SIZE) + integer (hoplength)
	int msgSize = (ID_LEN + 1) * (MAX_HOP + 2) + MAX_SIZE + INT_LEN;

	// buffer to hold message from server
	char* buf = calloc(msgSize, sizeof(char));

	int bytes = read(cli->fd, buf, msgSize);

	// client has disconnected
	if(bytes == 0){
		// TODO: handle disconnected client
		// need to remove client from clients list
	}

	struct message* m = parseMsg(buf, bytes);

	// if message is a datamessage
	if(strcmp(m->messageType, DATA_MSG) == 0){
		char* baseID = NULL;
		giveToBaseStation(baseID, m);
	}
	else if(strcmp(m->messageType, WHERE_MSG) == 0){

	}
	else if(strcmp(m->messageType, UPDATE_MSG) == 0){

	}


	free(cli);

	return NULL;
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

	// initialize a list of clients
	struct client* clientList = calloc(BACKLOG, sizeof(struct client));
	// set each client's file descriptor to special value no client, meaning no client has connected
	// in that index of the array
	for(int n = 0; n < BACKLOG; n++){
		clientList[n].fd = NO_CLIENT;
		clientList[n].site = NULL;
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

		// loop over connected clients and add to fdset
		for(int n = 0; n < BACKLOG; n++){
			// if a client has connected at this index
			if(clientList[n].fd != NO_CLIENT){
				// add that client's file descriptor to our fdset
				FD_SET(clientList[n].fd, &rfds);
				maxFd = clientList[n].fd > maxFd ? clientList[n].fd : maxFd;
			}
		}

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
		// loop over connected clients looking for message
		for(int n = 0; n < BACKLOG; n++){
			if(FD_ISSET(clientList[n].fd, &rfds)){
				// TODO: figure out what you want to do with disconnected clients

				// if a thread was previously created for this client, wait for that thread to finish
				//  and join the thread before creating a new thread
				if(clientList[n].tid != NO_THREAD){
					if(pthread_join(clientList[n].tid, NULL) != 0){
						perror("ERROR joining thread\n");
						return EXIT_FAILURE;
					}
					// after joining, set tid back to NO_THREAD
					clientList[n].tid = NO_THREAD;
				}

				// copy client struct into new memory to pass as arguments to thread
				void* args = calloc(1, sizeof(struct client));
				memcpy(args, &clientList[n], sizeof(struct client));
				pthread_create(&clientList[n].tid, NULL, handleMessage, args);
			}
		}



	}


	return 0;
}
/*
baseStationToBaseStation()

recvMsgFromClient()

serverToClient()

interactWithConsole() // getting command from user




*/