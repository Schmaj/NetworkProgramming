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

struct message;
struct hoplist;
char* msgToStr(struct message* msg, char* thisID);
struct baseStation;
struct siteLst;
void updateSiteLst(char* sensorID, int xPosition, int yPosition);

struct siteLst* globalSiteList;

int main(int argc, char * argv[]) {
	return 0;
}

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

struct baseStation{
	int xPos;
	int yPos;
	char* id;
	struct siteLst* connectedLst;
};

// new siteLst created for each baseStation - list of sites (clients will be added when they enter range)
struct siteLst{
	char* id;
	struct siteLst* next;
	int xPos;
	int yPos;
};

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
/*
baseStationToBaseStation()

recvMsgFromClient()

serverToClient()

interactWithConsole() // getting command from user




*/