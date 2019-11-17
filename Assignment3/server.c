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
#define TIMEOUT 25
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
#define REACHABLE_MSG "REACHABLE"

#define SERVER_ID "CONTROL"

// max characters in a site name
#define ID_LEN 64

struct message;
struct hoplist;
char* msgToStr(struct message* msg, char* thisID);
struct baseStation;
struct siteLst;
void updateSiteLst(char* sensorID, int xPosition, int yPosition);

struct siteLst* globalSiteList; // size MAX_BASE + MAX_CLIENTS
// mutex for accessing and editing globalSiteList
pthread_mutex_t siteListMutex = PTHREAD_MUTEX_INITIALIZER;
struct baseStation* globalBaseStationList; //10
// mutex for accessing and editing glbalBaseStationList
pthread_mutex_t baseListMutex = PTHREAD_MUTEX_INITIALIZER;
struct client* clientList;
// mutex for accessing and editing clientList
pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;


struct thread_args{
	struct client* cli;
	char* msg;
	int bytesRead;
};

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
	strcpy(retMsg->messageType, strtok(cpy, " \n\0"));

	if (strcmp(retMsg->messageType, "DATAMESSAGE") == 0){

		retMsg->originID = calloc(ID_LEN, sizeof(char));
		strcpy(retMsg->originID, strtok(NULL, " \n\0"));

		retMsg->nextID = calloc(ID_LEN, sizeof(char));
		strcpy(retMsg->nextID, strtok(NULL, " \n\0"));

		retMsg->destinationID = calloc(ID_LEN, sizeof(char));
		strcpy(retMsg->destinationID, strtok(NULL, " \n\0"));

		retMsg->hopLeng = atoi(strtok(NULL, " \n\0"));

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

	}
	return retMsg;
}

// initializes global list of base stations from base station file
//
// only called once, before parallelization so no need for mutex
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
	globalSiteList = calloc(1, sizeof(struct siteLst));
	struct siteLst* itr = globalSiteList;
	for (uint i = 0; i < MAX_BASE; ++i){
		if (globalBaseStationList[i].id == NULL) break;
		itr->id = calloc(ID_LEN+1, sizeof(char));
		strncpy(itr->id, globalBaseStationList[i].id, ID_LEN);
		itr->isBaseStation = 1;
		itr->xPos = globalBaseStationList[i].xPos;
		itr->yPos = globalBaseStationList[i].yPos;
		if (i == MAX_BASE-1 || globalBaseStationList[i+1].id == NULL){
			itr->next = NULL;
			break;
		}
		itr->next = calloc(1, sizeof(struct siteLst));
		itr = itr->next;
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
	//strcat(str, thisID);
	//strcat(str, " ");
	return str;
}


/*
Ret: void
Arg: msg is the message received. if msg->messageType == "Update Position", 
		update globalSiteList
*/
void updateSiteLst(char* sensorID, int xPosition, int yPosition){ // call that for everybody
	// lock siteList during update
	pthread_mutex_lock(&siteListMutex);
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
	pthread_mutex_unlock(&siteListMutex);
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
			//free(l->id);
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

	// lock for clientList access
	pthread_mutex_lock(&clientMutex);

	// not base station, forward message to correct client
	for(int n = 0; n < MAX_CLIENTS; n++){
		// if client has a name and name matches nextID
		if(clientList[n].site && strcmp(clientList[n].site->id, m->nextID) == 0){
			sd = clientList[n].fd;
			break;
		}
	}

	pthread_mutex_unlock(&clientMutex);

	if(sd == -1){
		printf("Client %s not found\n", m->nextID);
		return;
	}

	char* buf = msgToStr(m, "");

	int bytes = write(sd, buf, strlen(buf)+1);

	// TODO: double check error code
	if(bytes == 0){
		perror("write() failed\n");
	}

	freeMsg(m);


}


// for base station, finds id of next closest message to the destination, and updates fields in message accordingly
// (nextID, hoplen, hoplist)
void setNextID(char* myID, struct message* m, struct siteLst* reachableSites){
	struct siteLst* dest = NULL;
	pthread_mutex_lock(&siteListMutex);
	struct siteLst* iterator = globalSiteList;

	// find destination location
	while(iterator != NULL){
		if(m->destinationID == NULL || iterator->id == NULL){
			printf("bad cmp 1\n");
		}
		if(strcmp(m->destinationID, iterator->id) == 0){
			dest = iterator;
			break;
		}
		iterator = iterator->next;
	}

	pthread_mutex_unlock(&siteListMutex);

	if(dest == NULL){
		printf("%s: Message from %s to %s could not be delivered.\n", myID, m->originID, m->destinationID);
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
			if(itr->id == NULL || visited->id == NULL){
				printf("bad cmp 2\n");
			}
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
			if(itr->id == NULL || closestSite->id == NULL){
				printf("bad cmp 3\n");
			}
			// sets closestSite to the site with the lexicographically smaller name, between itr and closestSite
			closestSite = strcmp(itr->id, closestSite->id) < 0 ? itr : closestSite;
		}

		// move on to next site
		itr = itr->next;

	}

	if(closestSite == NULL){
		printf("%s: Message from %s to %s could not be delivered.\n", myID, m->originID, m->destinationID);
		m->nextID = NULL;
		return;
	}

	// mark next site to which message should be sent
	if(m->nextID != NULL){
		free(m->nextID);
		m->nextID = NULL;
	}
	m->nextID = calloc(ID_LEN + 1, sizeof(char));
	strcpy(m->nextID, closestSite->id);
	// add self to the hoplist
	for(struct hoplist* l = m->hoplst; 1; l = l->next){
		// just made message, hoplist uninitialized
		if(l == NULL){
			m->hoplst = calloc(1, sizeof(struct hoplist));
			m->hoplst->id = calloc(ID_LEN, sizeof(char));
			strcpy(m->hoplst->id, myID);
			break;
		}
		if(l->next == NULL){
			l->next = calloc(1, sizeof(struct hoplist));
			l->next->id = calloc(ID_LEN, sizeof(char));
			strcpy(l->next->id, myID);
			break;
		}
	}
	// increment hop length
	m->hopLeng++;

}

void giveToBaseStation(struct baseStation* base, struct message* m){
	// if this site is the destination
	if(strcmp(m->destinationID, base->id) == 0){
		// print that message was properly received
		printf("%s: Message from %s to %s successfully received.\n", base->id, m->originID, base->id);

		// message has been delivered, free memory and return
		freeMsg(m);

		return;
	}
	else{
		// print that message was properly received
		printf("%s: Message from %s to %s being forwarded through %s\n", base->id, m->originID, m->destinationID, base->id);
	}

	// finds and sets next site in path for message m
	setNextID(base->id, m, base->connectedLst);

	struct baseStation* baseNext = NULL;

	pthread_mutex_lock(&baseListMutex);

	if(m->nextID == NULL){
		return;
	}

	// find baseStation struct in baseStationList
	for(int n = 0; n < MAX_BASE; n++){
		// if base station initialized at index and name matches the next destination
		if(globalBaseStationList[n].id && strcmp(globalBaseStationList[n].id, m->nextID) == 0){
			// have the basestation receive message or hand it off to next recipient
			baseNext = &globalBaseStationList[n];
			pthread_mutex_unlock(&baseListMutex);

			giveToBaseStation(baseNext, m);

			// message has been dealt with, return
			return;
		}
	}

	pthread_mutex_unlock(&baseListMutex);

	// if not baseStation, send to client
	sendMsgOverSocket(m);



}

// if this id is a basestation return its index, else return -1
int isBaseStation(char* id){
	pthread_mutex_lock(&baseListMutex);
	for(int n = 0; n < MAX_BASE; n++){
		if(globalBaseStationList[n].id != NULL && strcmp(id, globalBaseStationList[n].id) == 0){
			pthread_mutex_unlock(&baseListMutex);
			return n;
		}
	}
	pthread_mutex_unlock(&baseListMutex);
	return -1;
}

// create string for response to client
// response includes list of all sites that are reachable by that client 
// message in form: REACHABLE [NumReachable] [ReachableList]
//		where ReachableList is space delimited list of form: [ID] [XPosition] [YPosition]
//
// also goes through each baseStation and removes or adds this to that baseStations connectedList, as appropriate
// returns dynamically allocated string ready to be sent over socket
char* getReachableList(char* id, int x, int y, int range){

	// square range because we are dealing with squared distance
	range = range * range;

	// estimate of max size of reachable list
	int estimate = MAX_SIZE + (INT_LEN + ID_LEN) * (MAX_BASE + MAX_CLIENTS + 1);

	// counts number of reachable sites
	int numReachable = 0;

	// allocate memory
	char* reachList = calloc(estimate, sizeof(char));

	pthread_mutex_lock(&siteListMutex);

	// iterate over every site we know about
	for(struct siteLst* itr = globalSiteList; itr != NULL; itr = itr->next){
		// do not consider adding this site to its own reachable list
		if(strcmp(id, itr->id) == 0){
			continue;
		}

		// find the square of distance between the two sites
		int dist = (x - itr->xPos)  * (x - itr->xPos) + (y - itr->yPos) * (y - itr->yPos);

		// if this site is reachable
		if(dist <= range){
			// add element to reachList, each entry is of form [ID] [XPosition] [YPosition]
			char entry[ID_LEN + 2*INT_LEN + 3];
			sprintf(entry, "%s %d %d ", itr->id, itr->xPos, itr->yPos);
			strcat(reachList, entry);
			numReachable++;
		}

		// if this site is a base station
		int base = isBaseStation(itr->id);
		if(base != -1){
			struct siteLst* connectedItr = globalBaseStationList[base].connectedLst;
			struct siteLst* prev = NULL;
			pthread_mutex_lock(&baseListMutex);
			while(1){

				// if this site is already in connected list
				if(strcmp(id, connectedItr->id) == 0){
					// if in range
					if(dist <= range){
						// update x and y position
						connectedItr->xPos = x;
						connectedItr->yPos = y;
					}
					else{
						// if this site is head of list, move head to next site
						if(prev == NULL){
							globalBaseStationList[base].connectedLst = connectedItr->next;
						}
						// otherwise update previous entry to point past this, removing it from the list
						else{
							prev->next = connectedItr->next;
						}

						// free memory
						free(connectedItr->id);
						free(connectedItr);

					}

					// done with loop
					break;
				}

				// if we have gone through whole list without finding, add to list if relevent
				if(connectedItr->next == NULL){
					// if we should add
					if(dist <= range){
						// add new site
						connectedItr->next = calloc(1, sizeof(struct siteLst));
						connectedItr->next->id = calloc(ID_LEN, sizeof(char));
						strcpy(connectedItr->next->id, id);
						connectedItr->next->xPos = x;
						connectedItr->next->yPos = y;
					}

					break;
				}

				prev = connectedItr;
				connectedItr = connectedItr->next;

				// TODO: look at this, leaving in hurry, probably not done

			}
			pthread_mutex_unlock(&baseListMutex);
		}
	}

	pthread_mutex_unlock(&siteListMutex);

	char* msg = calloc(estimate, sizeof(char));

	// message begins with "REACHABLE [NumReachable]"
	sprintf(msg, "%s %d ", REACHABLE_MSG, numReachable);
	// append reachable list
	strcat(msg, reachList);

	free(reachList);

	return msg;

}

void* handleMessage(void* args){
	//printf("begin handle message\n");
	struct client* cli = (struct client*)(((struct thread_args*)args)->cli);
	int bytes = ((struct thread_args*)args)->bytesRead;
	char* buf = ((struct thread_args*)args)->msg;

	// create message struct from string buffer
	struct message* m = parseMsg(buf, bytes);


	// if message is a datamessage
	if(strcmp(m->messageType, DATA_MSG) == 0){

		// if server is destination
		if(strcmp(m->destinationID, SERVER_ID) == 0){
			printf("%s: Message from %s to %s successfully received.\n", SERVER_ID, m->originID, SERVER_ID);
			// message has been delivered, free memory and return
			freeMsg(m);
			free(cli);
			free(buf);
			free(args);
			return NULL;
		}

		struct baseStation* base = NULL;
		pthread_mutex_lock(&baseListMutex);
		// find baseStation struct in baseStationList
		for(int n = 0; n < MAX_BASE; n++){
			// if base station initialized at index and name matches the next destination
			if(globalBaseStationList[n].id && strcmp(globalBaseStationList[n].id, m->nextID) == 0){
				// have the basestation receive message or hand it off to next recipient
				base = &globalBaseStationList[n];
				pthread_mutex_unlock(&baseListMutex);
				giveToBaseStation(base, m);

				// message has been dealt with, return
				free(cli);
				free(buf);
				free(args);
				return NULL;
			}
		}
		pthread_mutex_unlock(&baseListMutex);
		// sends message to appropriate client
		sendMsgOverSocket(m);


	}
	else if(strcmp(m->messageType, WHERE_MSG) == 0){
		//MAY HAVE TO PUT THIS INTO A FUNCTION
		char* NodeID = calloc(ID_LEN+1, sizeof(char));
		strncpy(NodeID, strtok(buf, " "),ID_LEN+1);
		if (strcmp(NodeID, "WHERE") == 0){
			memset(NodeID, 0, ID_LEN+1);
			strncpy(NodeID, strtok(NULL, " \n\0"), ID_LEN);
		}
		int XPosition = -1, YPosition = -1;
		pthread_mutex_lock(&siteListMutex);
		struct siteLst* itr = globalSiteList;
		while(itr){
			if (strcmp(itr->id, NodeID) == 0){
				XPosition = itr->xPos;
				YPosition = itr->yPos;
				break;
			}
			itr = itr->next;
		}
		pthread_mutex_unlock(&siteListMutex);
		if (XPosition == -1 || YPosition == -1){
			perror("WHERE_MSG, Position");
			exit(EXIT_FAILURE);
		}
		char* msgOut = calloc(ID_LEN*2 + 50, sizeof(char));
		sprintf(msgOut, "THERE %s %d %d ", NodeID, XPosition, YPosition);
		free(NodeID);
		int retno = write(cli->fd, msgOut, strlen(msgOut)+1);
		if (retno <= 0){
			perror("WHERE, write");
			exit(EXIT_FAILURE);
		}
		free(msgOut);
	}
	else if(strcmp(m->messageType, UPDATE_MSG) == 0){
		//printf("begin update message\n");

		//"UPDATEPOSITION %s %d %d %d ", sensorID, SensorRange, xPos, yPos
		// move pointer of strtok past "UPDATEPOSITION" in our buffer
		strtok(buf, " \n\0");

		char* newId = calloc(ID_LEN, sizeof(char));

		// copy id from message into newId
		strcpy(newId, strtok(NULL, " "));

		struct siteLst* updateSite = globalSiteList;

		if(globalSiteList == NULL){
			printf("NULL globalSiteList\n");
			return 0;
		}

		// search through existing sites to determine if this is a new site or an update
		// if new site, create new struct siteLst for it
		// upon exiting this loop, updateSite will point to the site of this client
		while(1){

			if(updateSite->id == NULL){
				printf("NULL id\n");
				return 0;
			}

			// if this site already exists, break
			if(strcmp(updateSite->id, newId) == 0){
				break;
			}

			// if there is no next site, this site does not exist yet, create it
			if(updateSite->next == NULL){
				
				// allocate memory for site and site id, and copy over new id
				updateSite->next = calloc(1, sizeof(struct siteLst));
				updateSite->next->id = calloc(ID_LEN, sizeof(char));
				strcpy(updateSite->next->id, newId);

				// set field in appropriate client to point to this new site
				for(int n = 0; n < MAX_CLIENTS; n++){
					if(cli->fd == clientList[n].fd){
						clientList[n].site = updateSite->next;
						break;
					}
				}
				// set updateSite and break
				updateSite = updateSite->next;
				break;
			}

			updateSite = updateSite->next;
		}

		// get range from message
		int range = atoi(strtok(NULL, " \n\0"));

		// update xPos and yPos fields in our globalSiteList
		int newX = atoi(strtok(NULL, " \n\0"));
		int newY = atoi(strtok(NULL, " \n\0"));

		updateSite->xPos = newX;
		updateSite->yPos = newY;

		// create string for response to client
		// response includes list of all sites that are reachable by that client 
		// message in form: REACHABLE [NumReachable] [ReachableList]
		//		where ReachableList is space delimited list of form: [ID] [XPosition] [YPosition]
		char* response = getReachableList(updateSite->id, newX, newY, range);

		write(cli->fd, response, strlen(response)+1);

		free(response);


	}


	free(cli);
	free(buf);
	free(args);
	return NULL;
}

int interactWithConsole(){
	// buffer to hold command
	char buf[CMD_SIZE];

	// read next line from command line into buf
	fgets(buf, CMD_SIZE, stdin);

	char command[CMD_SIZE];

	strcpy(command, strtok(buf, " \n\0"));
	// SENDDATA [DestinationID]
	if(strcmp(command, SEND_CMD) == 0){

		struct message* m = calloc(1, sizeof(struct message));

		// initialize messageType
		m->messageType = calloc(ID_LEN+1, sizeof(char));
		strncpy(m->messageType, DATA_MSG, ID_LEN);

		m->originID = calloc(ID_LEN+1, sizeof(char));
		strncpy(m->originID, strtok(NULL, " \n\0"), ID_LEN);

		// nextID initially null, will be determined 
		m->nextID = NULL;

		// destination ID intialized to destination given by user
		m->destinationID = calloc(ID_LEN+1, sizeof(char));
		strncpy(m->destinationID, strtok(NULL, " \n\0"), ID_LEN);

		// hopleng and hoplist initially empty, will be updated to include self in sendDataMsg()
		m->hopLeng = 0;
		m->hoplst = NULL;

		if (strcmp(m->originID, CONTROL) == 0){
			sendMsgOverSocket(m);
		} else { //base station
			struct baseStation* base = NULL;

			// find baseStation struct in baseStationList
			for(int n = 0; n < MAX_BASE; n++){
				// if base station initialized at index and name matches the next destination
				if(globalBaseStationList[n].id && strcmp(globalBaseStationList[n].id, m->originID) == 0){
					// have the basestation receive message or hand it off to next recipient
					base = &globalBaseStationList[n];
					break;
				}
			}
			giveToBaseStation(base, m);
		}

		// free message
		freeMsg(m);

		return 0;


	}
	// QUIT
	else if(strncmp(command, QUIT_CMD, strlen(QUIT_CMD)) == 0){
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

	setvbuf(stdout, NULL, _IONBF, 0);
	
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
	clientList = calloc(MAX_CLIENTS, sizeof(struct client));
	// set each client's file descriptor to special value no client, meaning no client has connected
	// in that index of the array
	for(int n = 0; n < MAX_CLIENTS; n++){
		clientList[n].fd = NO_CLIENT;
		clientList[n].tid = NO_THREAD;
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
		for(int n = 0; n < MAX_CLIENTS; n++){
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
		int retval = select(maxFd + 1, &rfds, NULL, NULL, &timeout);

		if(retval == 0){
			//printf("No Activity\n");
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

				for(int n = 0; n < MAX_CLIENTS; n++){

					// allow all threads to finish and join them
					if(clientList[n].tid != NO_THREAD){
						pthread_join(clientList[n].tid, NULL);
						clientList[n].tid = NO_THREAD;
					}
					// disconnect from clients and close sockets
					if(clientList[n].fd != NO_CLIENT){
						close(clientList[n].fd);
						clientList[n].fd = NO_CLIENT;
					}
				}

				close(listenerFd);
				free(clientList);
				free(globalBaseStationList);
				//TODO: free globalSiteList

				return 0;
			}
		}
		// new client connecting
		if(FD_ISSET(listenerFd, &rfds)){

			int index = -1;
			// loop over client list and find next available index
			for(int n = 0; n < MAX_CLIENTS; n++){
				if(clientList[n].fd == NO_CLIENT){
					index = n;
					break;
				}
			}

			// if no more spaces available, print error
			if(index == -1){
				perror("ERROR: max clients already connected\n");
				return EXIT_FAILURE;
			}

			// accept connection and fill in client list
			clientList[index].fd = accept(listenerFd, NULL, NULL);
			clientList[index].tid = NO_THREAD;
			clientList[index].site = NULL;

			//printf("Accepted connection\n");
		}
		// loop over connected clients looking for message
		for(int n = 0; n < MAX_CLIENTS; n++){
			// if client has disconnected, join final thread for that client
			if(clientList[n].fd == NO_CLIENT && clientList[n].tid != NO_THREAD){
				//printf("Joining\n");
				if(pthread_join(clientList[n].tid, NULL) != 0){
						perror("ERROR joining thread\n");
						return EXIT_FAILURE;
					}
					// after joining, set tid back to NO_THREAD
					clientList[n].tid = NO_THREAD;
			}
			if(clientList[n].fd != NO_CLIENT && FD_ISSET(clientList[n].fd, &rfds)){

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
				struct thread_args* args = calloc(1, sizeof(struct thread_args));
				args->cli = calloc(1, sizeof(struct client));
				memcpy(args->cli, &clientList[n], sizeof(struct client));


				// estimated max size of message
				// length of name (plus 1 character for space) multiplied by the max number of names (full hop list + originalSiteId
				// + destinationSiteID) + message type length (MAX_SIZE) + integer (hoplength)
				int msgSize = (ID_LEN + 1) * (MAX_HOP + 2) + MAX_SIZE + INT_LEN;

				// buffer to hold message from server
				char* buf = calloc(msgSize, sizeof(char));

				int bytes = read(args->cli->fd, buf, msgSize);

				// client has disconnected
				if(bytes == 0){

					struct client* cli = args->cli;
					//printf("Client disconnected\n");
					// need to remove client from clients list

					for(int n = 0; n < MAX_CLIENTS; n++){
						if(cli->fd == clientList[n].fd){
							// set all fields to values representing no client
							close(clientList[n].fd);
							clientList[n].fd = NO_CLIENT;
							clientList[n].site = NULL;
							break;
						}
					}

					//clientList[n].fd = NO_CLIENT;

					free(buf);
					free(cli);
					free(args);

					continue;
				}

				args->bytesRead = bytes;
				args->msg = buf;
				pthread_create(&clientList[n].tid, NULL, handleMessage, (void*)args);
			}
		}



	}


	return 0;
}
