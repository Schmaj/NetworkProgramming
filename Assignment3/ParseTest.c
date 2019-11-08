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
#include <arpa/inet.h>

#define MAX_SIZE 30

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
		return retMsg;
	} else {
		printf("THERE\n");

		free(cpy);
	}

	return retMsg;
}

int main(int argc, char * argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	struct message * test = parseMsg("DATAMESSAGE originID nextID destinationID 2 HopA HopB ", 
		strlen("DATAMESSAGE originID nextID destinationID 2 HopA HopB ")+1);

	printf("%s\n", test->messageType);
	printf("%s\n", test->originID);
	printf("%s\n", test->nextID);
	printf("%s\n", test->destinationID);
	printf("%d\n", test->hopLeng);
	struct hoplist* iterator = test->hoplst;
	while(iterator){
		printf("%s\n", iterator->id);
		iterator = iterator->next;
	}
	free(test);
	return 0;
}