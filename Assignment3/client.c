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
#include <arpa/inet.h>
#include <limits.h>

// max characters in an address
#define ADDRESS_LEN 64
// max characters in a site name
#define ID_LEN 64
// number of seconds to wait for select() call
#define TIMEOUT 25
// size of buffer for command line commands
#define CMD_SIZE 256
// max number of characters in xPos or yPos for MOVE command
#define INT_LEN 16
//max number of bytes to be read in
#define MAX_REACHABLE 32

// maximum expected number of hops in a hop list
#define MAX_HOP 16

#define MAX_SIZE 64

// string literals for each command
#define MOVE_CMD "MOVE"
#define SEND_CMD "SENDDATA"
#define QUIT_CMD "QUIT\n"
#define WHERE_CMD "WHERE"

#define DATA_MSG "DATAMESSAGE"


char* THIS_ID;

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
Ret: string to be send, dynamically stored
*/
char* msgToStr(struct message* msg){
	int msg_size = strlen(msg->messageType) + strlen(msg->originID) + strlen(msg->nextID);
	msg_size += strlen(msg->destinationID) + (int)ceil(log10(msg->hopLeng))+6;
	struct hoplist* iterator = msg->hoplst;
	while (iterator != NULL){
		msg_size += strlen(iterator->id) + 1;
		iterator = iterator->next;
	}
	msg_size += strlen(THIS_ID) + 1;

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
	//strcat(str, THIS_ID);
	//strcat(str, " ");
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
	free(lst->id);
	free(lst);
	return;
}


// takes in message m, fills in nextID from reachableSites and destinationID, and sends appropriate message to server
// adds self to hoplist at the end of this function
void sendDataMsg(char* myID, int sockfd, struct message* m, struct siteLst* reachableSites, struct siteLst* dest){

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
		printf("Message from %s to %s cannot be delivered.\n", m->originID, m->destinationID);
		return;
	}

	// mark next site to which message should be sent
	if(m->nextID != NULL){
		free(m->nextID);
		m->nextID = NULL;
	}
	m->nextID = calloc(ID_LEN+1, sizeof(char));
	strncpy(m->nextID, closestSite->id, ID_LEN);
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

	char* msg = msgToStr(m);

	if(strcmp(myID, m->originID) == 0){
		printf("%s: Sent a new message bound for %s.\n", THIS_ID, m->destinationID);
	}
	else{
		printf("%s: message from %s to %s being forwarded through %s\n", myID, m->originID, m->destinationID, myID);
	}
	// send message to server, add one byte for null terminator
	write(sockfd, msg, strlen(msg) + 1);

	free(msg);
	msg = NULL;


}


// sends new position message to server and waits for REACHABLE response, reinitializing lst
// returns pointer to new dynamically allocated linked list of siteLst
//
// lst: siteLst of all reachable sites.  If not null, deallocated
struct siteLst* updatePosition(struct siteLst* lst, char* sensorID, int SensorRange, int xPos, int yPos, int fd){

	// deallocate old list, if it exists
	if(lst){
		freeLst(lst);
	}

	char* msg = calloc(strlen("UPDATEPOSITION ")+1 + ID_LEN + INT_LEN*3 + 4, sizeof(char));
	sprintf(msg, "UPDATEPOSITION %s %d %d %d ", sensorID, SensorRange, xPos, yPos);

	int retno = write(fd, msg, strlen(msg)+1);
	if (retno <= 0){
		perror("updatePosition, write");
		exit(EXIT_FAILURE);
	}
	free(msg);
	msg = calloc(MAX_REACHABLE*ID_LEN + 20, sizeof(char));
try_read:
	retno = read(fd, msg, MAX_REACHABLE*ID_LEN + 19);
	if (retno <= 0){
		perror("updatePosition, read");
		exit(EXIT_FAILURE);
	}

	if(retno < 10){
		printf("Using goto, first byte is ");
		if(*msg == '\0'){
			printf("NULL\n");
		}
		else if(*msg == ' '){
			printf("space\n");
		}
		goto try_read;
	}

	char* messageType = strtok(msg, " \0\n");
	if(messageType == NULL){
		printf("NULL message in update\n, retno: %d\n", retno);

	}
	if (strcmp(messageType, "REACHABLE") != 0){
		perror("updatePosition, messageType");
		exit(EXIT_FAILURE);
	}
	int numReachable = atoi(strtok(NULL, " "));
	lst = calloc(1, sizeof(struct siteLst));
	struct siteLst* iterator = lst;

	for (unsigned int i = 0; i < numReachable; ++i){
		iterator->id = calloc(ID_LEN+1, sizeof(char));
		strcpy(iterator->id, strtok(NULL, " "));
		iterator->xPos = atoi(strtok(NULL, " "));
		iterator->yPos = atoi(strtok(NULL, " "));
		if (i == numReachable-1){
			iterator->next = NULL;
		} else {
			iterator->next = calloc(1, sizeof(struct siteLst));
			iterator = iterator->next;
		}
	}
	return lst;
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
			//struct sockaddr_in* info = (struct sockaddr_in*)iterator->ai_addr;
			// check port is correct
			//if(info->sin_port == controlPort){

			struct sockaddr_in info;

			memset(&info, 0, sizeof(info));
			info.sin_family = AF_INET;
			info.sin_port = htons(controlPort);
			info.sin_addr.s_addr = ((*(struct sockaddr_in*)((*iterator).ai_addr)).sin_addr).s_addr;


				if(connect(sockfd, (struct sockaddr*)&info, sizeof(info)) != 0){
					perror("ERROR: connect() failed\n");
				}
				// successful connect, return value will be 0
				else{
					status = 0;
					break;
				}
			//}

		} 
		// IPv6
		else if(family == AF_INET6){
			struct sockaddr_in6* info = (struct sockaddr_in6*)iterator->ai_addr;	
			// check port is correct
			//if(info->sin6_port == controlPort){

				if(connect(sockfd, (struct sockaddr*)info, sizeof(info)) != 0){
					perror("ERROR: connect() failed\n");
					break;
				}
				// successful connect, return value will be 0
				status = 0;
				break;
			//}
		}
		else{
			printf("Unrecognized family\n");
		}

		iterator = iterator->ai_next;
	}

	freeaddrinfo(servinfo);

	return status;

}


// returns dynamically allocated thereSite, siteLst of the requested site
struct siteLst* where(int sockfd, struct siteLst* reachableSites, int xPos, int yPos, int SensorRange, char* whereID){

	char* msg = calloc(2*ID_LEN + 1, sizeof(char));
	sprintf(msg, "WHERE %s ", whereID);
	int retno = write(sockfd, msg, 2*ID_LEN);
	if (retno <= 0){
		perror("interactWithConsole, WHERE, write");
		exit(EXIT_FAILURE);
	}
	memset(msg, 0, 2*ID_LEN + 2);
	retno = read(sockfd, msg, 2*ID_LEN + 1);
	if (retno <= 0){
		perror("interactWithConsole, WHERE, read");
		exit(EXIT_FAILURE);
	}

	char* messageType = calloc(ID_LEN+1, sizeof(char));
	strcpy(messageType, strtok(msg, " "));
	if (strcmp(messageType, "THERE") != 0){
		perror("interactWithConsole, WHERE, messageType");
		exit(EXIT_FAILURE);
	}
	free(messageType);

	struct siteLst* thereSite = calloc(1, sizeof(struct siteLst));
	thereSite->id = calloc(ID_LEN+1, sizeof(char));
	strncpy(thereSite->id, strtok(NULL, " "), ID_LEN);
	thereSite->xPos = atoi(strtok(NULL, " "));
	thereSite->yPos = atoi(strtok(NULL, " "));

	
	free(msg);
	free(whereID);

	struct siteLst* iterator = reachableSites;
	//reachable
	if (pow(xPos - thereSite->xPos, 2) + pow(yPos - thereSite->yPos, 2) <= pow(SensorRange, 2)){
		if (!iterator){
			reachableSites = calloc(1, sizeof(struct siteLst));
			reachableSites->id = calloc(strlen(thereSite->id)+1, sizeof(char));
			strcpy(reachableSites->id, thereSite->id);
			reachableSites->xPos = thereSite->xPos;
			reachableSites->yPos = thereSite->yPos;
		} else {
			while(iterator->next && strcmp(iterator->id, thereSite->id) != 0){
				iterator = iterator->next;
			}
			if (strcmp(iterator->id, thereSite->id) != 0){ //change position
				iterator->xPos = thereSite->xPos;
				iterator->yPos = thereSite->yPos;
			} else { //have to add it to the end
				iterator->next = calloc(1, sizeof(struct siteLst));
				iterator = iterator->next;
				iterator->id = calloc(strlen(thereSite->id)+1, sizeof(char));
				strcpy(iterator->id, thereSite->id);
				iterator->xPos = thereSite->xPos;
				iterator->yPos = thereSite->yPos;
			}
		}
	}

	return thereSite;
}

// takes in command from stdin and executes user command

/* Commands include:

MOVE [NewXPosition] [NewYPosition]
SENDDATA [DestinationID]
QUIT
WHERE [SensorID/BaseID]

*/
int interactWithConsole(char* sensorID, int sockfd, int SensorRange, struct siteLst** reachableSitesPtr, struct siteLst* knownLocations, 
				int* xPtr, int* yPtr){

	struct siteLst* reachableSites = *reachableSitesPtr;

	int xPos = *xPtr;
	int yPos = *yPtr;

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
		strcpy(pos, strtok(NULL, " "));
		int newxPos = atoi(pos);
		// 0 out buffer and copy yposition
		memset(pos, '\0', INT_LEN);
		strcpy(pos, strtok(NULL, " \0"));
		int newyPos = atoi(pos);

		*xPtr = newxPos;
		*yPtr = newyPos;

		// update position and send message to server
		updatePosition(reachableSites, sensorID, SensorRange, newxPos, newyPos, sockfd);
		*reachableSitesPtr = reachableSites;

		return 0;

	}
	// SENDDATA [DestinationID]
	else if(strcmp(command, SEND_CMD) == 0){

		struct message* m = calloc(1, sizeof(struct message));

		// initialize messageType
		m->messageType = calloc(ID_LEN+1, sizeof(char));
		strncpy(m->messageType, DATA_MSG, ID_LEN);

		m->originID = calloc(ID_LEN+1, sizeof(char));
		strncpy(m->originID, sensorID, ID_LEN);

		// nextID initially null, will be determined 
		m->nextID = NULL;

		// destination ID intialized to destination given by user
		m->destinationID = calloc(ID_LEN+1, sizeof(char));
		strncpy(m->destinationID, strtok(NULL, "\n"), ID_LEN);

		// hopleng and hoplist initially empty, will be updated to include self in sendDataMsg()
		m->hopLeng = 0;
		m->hoplst = NULL;

		char* whereID = calloc(ID_LEN+1, sizeof(char));
		strncpy(whereID, m->destinationID, ID_LEN);

		// update position ot get up to date reachable list
		reachableSites = updatePosition(reachableSites, sensorID, SensorRange, xPos, yPos, sockfd);
		*reachableSitesPtr = reachableSites;
		struct siteLst* dest = where(sockfd, reachableSites, xPos, yPos, SensorRange, whereID);

		sendDataMsg(sensorID, sockfd, m, reachableSites, dest);

		free(dest);

		// TODO free message
		freeMsg(m);

		return 0;


	}
	// QUIT
	else if(strcmp(command, QUIT_CMD) == 0){
		// return value of 1 signals quit, all real cleanup will happen in main
		return 1;
		
	}
	// WHERE [SensorID/BaseID]
	else if(strcmp(command, WHERE_CMD) == 0){
		char* whereID = calloc(ID_LEN+1, sizeof(char));
		strncpy(whereID, strtok(NULL, " \0\n"), ID_LEN);
		where(sockfd, reachableSites, xPos, yPos, SensorRange, whereID);
	}

	return 0;

}

// receive a message from socket
int recvMsg(int sockfd, char* myID, struct siteLst** reachableSitesPtr, int xPos, int yPos, int SensorRange){

	struct siteLst* reachableSites = *reachableSitesPtr;

	// estimated max size of message
	// length of name (plus 1 character for space) multiplied by the max number of names (full hop list + originalSiteId
	// + destinationSiteID) + message type length (MAX_SIZE) + integer (hoplength)
	int msgSize = (ID_LEN + 1) * (MAX_HOP + 2) + MAX_SIZE + INT_LEN;

	// buffer to hold message from server
	char* buf = calloc(msgSize, sizeof(char));

	int bytes = read(sockfd, buf, msgSize);
	// other side closed connection, clean up
	if(bytes == 0){
		close(sockfd);
	}

	struct message* m = parseMsg(buf, bytes);

	// if this site is the destination
	if(strcmp(m->destinationID, myID) == 0){
		// print that message was properly received
		printf("%s: Message from %s to %s successfully received\n", myID, m->originID, myID);
		return 0;
	}

	char* whereID = calloc(ID_LEN+1, sizeof(char));
	strncpy(whereID, m->destinationID, ID_LEN);

	// update position ot get up to date reachable list
	reachableSites = updatePosition(reachableSites, myID, SensorRange, xPos, yPos, sockfd);
	*reachableSitesPtr = reachableSites;
	struct siteLst* dest = where(sockfd, reachableSites, xPos, yPos, SensorRange, whereID);

	sendDataMsg(myID, sockfd, m, reachableSites, dest);

	free(dest);

	return 0;
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
	THIS_ID = calloc(ID_LEN+1, sizeof(char));
	strncpy(THIS_ID, argv[3], ID_LEN);
	
	// can communicate only to other sites in this range
	int SensorRange = atoi(argv[4]);
	
	// x and y coordinates
	int xPos = atoi(argv[5]);
	int yPos = atoi(argv[6]);


	// make socket, try to connect

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);

	// resolves server name and connects socket to server
	if(connectToServer(sockfd, controlAddress, controlPort) == 1){
		printf("Could not connect to server\n");
		return 0;
	}

	// control address no longer needed, free memory
	free(controlAddress);
	controlAddress = NULL;


	// initialize list of reachable sites to empty
	struct siteLst* reachableSites = NULL;

	// sends initial updatePosition message and initializes list of reachable sites
	updatePosition(reachableSites, sensorID, SensorRange, xPos, yPos, sockfd);

	// initialize list of all sites with known location
	struct siteLst* knownLocations = NULL;
	struct siteLst* tmpItr = NULL;

	// copy reachableSites into knownLocations
	for(struct siteLst* itr = reachableSites; itr != NULL; itr = itr->next){
		// initialize first site, set knownLocations to the head permanently
		if(knownLocations == NULL){
			knownLocations = calloc(1, sizeof(struct siteLst));
			tmpItr = knownLocations;
		}
		else{
			tmpItr->next = calloc(1, sizeof(struct siteLst));
			tmpItr = tmpItr->next;
		}

		// initialize name
		tmpItr->id = calloc(ID_LEN, sizeof(char));
		strcpy(tmpItr->id, itr->id);

		// copy position
		tmpItr->xPos = itr->xPos;
		tmpItr->yPos = itr->yPos;
	}


	// ready to loop?

	// structure to hold a set of file descriptors to watch
	fd_set rfds;

	// file descriptor for stdin, could change based on piping or other output shenanigans
	int standardInput = fileno(stdin);

	// flag for CONTROL having disconnected, don't poll sockfd anymore
	int closedSock = 0;

	while(1){


		// set rfds to include no file descriptors
		FD_ZERO(&rfds);

		// add stdin to fdset
		FD_SET(standardInput, &rfds);

		// if socket still communicating
		if(closedSock == 0){
			// add socket for server to fdset
			FD_SET(sockfd, &rfds);
		}

		// structure to specify TIMEOUT second timeout on select() call
		struct timeval timeout;

		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = 0;

		// wait for activity on listening socket, or any active client
		int retval = select(sockfd > standardInput ? sockfd + 1 : standardInput + 1, &rfds, NULL, NULL, &timeout);

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
			
			int quit = interactWithConsole(sensorID, sockfd, SensorRange, &reachableSites, knownLocations, &xPos, &yPos);

			// quit command received, release memory and close sockets
			if(quit == 1){
				close(sockfd);

				free(sensorID);
				sensorID = NULL;

				freeLst(reachableSites);
				freeLst(knownLocations);

				return 0;

			}
		}

		// if socket still open
		if(closedSock == 0){

			if(FD_ISSET(sockfd, &rfds)){
				//printf("RECEIVED something\n");
				int rc = recvMsg(sockfd, sensorID, &reachableSites, xPos, yPos, SensorRange);
				if(rc == 5){
					closedSock = 1;
					sockfd = -1;
				}

			}

		}

	}


	return 0;
}
