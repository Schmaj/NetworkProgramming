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
#include <limits.h>

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
// maximum number of base stations
#define MAX_BASE 10

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
#define QUIT_CMD "QUIT"
#define CONTROL "CONTROL"

#define SERVER_ID "CONTROL"

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
struct client* clientList;


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

// frees all dynamic memory in a message m, including m itself
void freeMsg(struct message* m){
	if(m->messageType){
		free(m->messageType);
		m->messageType = NULL;
	}
	if(m->originID){
		free(m->originID);
		m->originID = NULL;
	}
	if(m->nextID){
		free(m->nextID);
		m->nextID = NULL;
	}
	if(m->destinationID){
		free(m->destinationID);
		m->destinationID = NULL;
	}
	struct hoplist* l = m->hoplst;
	while(l != NULL){

		if(l->id){
			free(l->id);
			l->id = NULL;
		}

		struct hoplist* tmp = l;
		l = l->next;
		free(tmp);
	}

	free(m);
}

// only called if next recipient is known and not a base station
// actually sends data over socket, frees message struct m
void sendMsgOverSocket(struct message* m){

	// socket descriptor for client
	int sd = -1;

	// not base station, forward message to correct client
	for(int n = 0; n < MAX_CLIENTS; n++){
		// if client has a name and name matches nextID
		if(clientList[n].site && strcmp(clientList[n].site->id, m->nextID) == 0){
			sd = clientList[n].fd;
			break;
		}
	}

	if(sd == -1){
		printf("Client %s not found\n", m->nextID);
		return;
	}

	char* buf = msgToStr(m, "");

	int bytes = write(sd, buf, strlen(buf));


	// TODO: double check error code
	if(bytes == 0){
		perror("write() failed\n");
	}

	free(m);


}


// for base station, finds id of next closest message to the destination, and updates fields in message accordingly
// (nextID, hoplen, hoplist)
void setNextID(char* myID, struct message* m, struct siteLst* reachableSites){
	struct siteLst* dest = NULL;
	struct siteLst* iterator = globalSiteList;

	// find destination location
	while(iterator != NULL){
		if(strcmp(m->destinationID, iterator->id) == 0){
			dest = iterator;
			break;
		}
		iterator = iterator->next;
	}

	if(dest == NULL){
		printf("Do not know location of site %s\n", m->destinationID);
		return;
	}

	// closest to dest, ties alphabetically
	struct siteLst* closestSite = NULL;
	int closestDist = INT_MAX;

	// pointer to walk through list of reachable sites
	struct siteLst* itr = reachableSites;
	// loop over reachable sites and find site closest to destination that would not cause a cycle
	while(itr != NULL){

		// flag representing whether or not site itr is on the hoplst
		int skip = 0;

		for(struct hoplist* visited = m->hoplst; visited != NULL; visited = visited->next){
			if(strcmp(itr->id, visited->id) == 0){
				skip = 1;
				break;
			}
		}

		// if itr is on hoplst, sending message would cause cycle, skip and consider next site
		if(skip == 1){
			itr = itr->next;
			continue;
		}

		// square of distance from itr to dest
		int dist = (dest->xPos - itr->xPos)  * (dest->xPos - itr->xPos) + (dest->yPos - itr->yPos) * (dest->yPos - itr->yPos);

		// update closest values
		if(dist < closestDist){
			closestSite = itr;
			closestDist = dist;
		}
		// distance is equal, settle tie in lexicographical order
		else if(dist == closestDist){
			// sets closestSite to the site with the lexicographically smaller name, between itr and closestSite
			closestSite = strcmp(itr->id, closestSite->id) < 0 ? itr : closestSite;
		}

		// move on to next site
		itr = itr->next;

	}

	if(closestSite == NULL){
		// TODO: change to proper print statement for message chosen not to send
		printf("No closest site, not sending message\n");
		return;
	}

	// mark next site to which message should be sent
	if(m->nextID != NULL){
		free(m->nextID);
		m->nextID = NULL;
	}
	m->nextID = calloc(ID_LEN, sizeof(char));
	strcpy(m->nextID, closestSite->id);
	// add self to the hoplist
	for(struct hoplist* l = m->hoplst; 1; l = l->next){
		// just made message, hoplist uninitialized
		if(l == NULL){
			m->hoplst = calloc(1, sizeof(struct hoplist));
			m->hoplst->id = calloc(ID_LEN, sizeof(char));
			strcpy(m->hoplst->id, myID);
		}
		if(l->next == NULL){
			l->next = calloc(1, sizeof(struct hoplist));
			l->next->id = calloc(ID_LEN, sizeof(char));
			strcpy(l->next->id, myID);
		}
	}
	// increment hop length
	m->hopLeng++;
}

void giveToBaseStation(struct baseStation* base, struct message* m){
	// if this site is the destination
	if(strcmp(m->destinationID, base->id) == 0){
		// print that message was properly received
		printf("%s: Message from %s to %s successfully received\n", base->id, m->originID, base->id);

		// message has been delivered, free memory and return
		free(m);

		return;
	}

	// finds and sets next site in path for message m
	setNextID(base->id, m, base->connectedLst);

	struct baseStation* baseNext = NULL;

	// find baseStation struct in baseStationList
	for(int n = 0; n < MAX_BASE; n++){
		// if base station initialized at index and name matches the next destination
		if(globalBaseStationList[n].id && strcmp(globalBaseStationList[n].id, m->nextID) == 0){
			// have the basestation receive message or hand it off to next recipient
			baseNext = &globalBaseStationList[n];
			giveToBaseStation(baseNext, m);

			// message has been dealt with, return
			return;
		}
	}

	// if not baseStation, send to client
	sendMsgOverSocket(m);



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

		// if server is destination
		if(strcmp(m->destinationID, SERVER_ID) == 0){
			printf("%s: Message from %s to %s successfully received\n", SERVER_ID, m->originID, SERVER_ID);
			// message has been delivered, free memory and return
			free(m);
			free(cli);
			return NULL;
		}

		struct baseStation* base = NULL;

		// find baseStation struct in baseStationList
		for(int n = 0; n < MAX_BASE; n++){
			// if base station initialized at index and name matches the next destination
			if(globalBaseStationList[n].id && strcmp(globalBaseStationList[n].id, m->nextID) == 0){
				// have the basestation receive message or hand it off to next recipient
				base = &globalBaseStationList[n];
				giveToBaseStation(base, m);

				// message has been dealt with, return
				free(cli);
				return NULL;
			}
		}

		// sends message to appropriate client
		sendMsgOverSocket(m);


	}
	else if(strcmp(m->messageType, WHERE_MSG) == 0){

	}
	else if(strcmp(m->messageType, UPDATE_MSG) == 0){

	}


	free(cli);

	return NULL;
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
		strcpy(m->destinationID, strtok(NULL, "\n"));

		// hopleng and hoplist initially empty, will be updated to include self in sendDataMsg()
		m->hopLeng = 0;
		m->hoplst = NULL;

		if (strcmp(m->originID, CONTROL) == 0){
			handleMessage(NULL);
		} else { //base station
			giveToBaseStation(m->originID, m);
		}

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
	clientList = calloc(BACKLOG, sizeof(struct client));
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