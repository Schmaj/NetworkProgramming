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

struct message * parseMsg(char * msg){
	char * cpy = calloc(strlen(msg)+1, sizeof(char));
	strcpy(cpy, msg);

	char * keyword = strtok(cpy, " ");
	if (strcmp(keyword, "DATAMESSAGE") == 0){
		printf("DATAMESSAGE\n");
		free(cpy);
	} else if (strcmp(keyword, "THERE") == 0){
		printf("THERE\n");
		free(cpy);
	}

	struct message * empty = calloc(1, sizeof(struct message));
	return empty;
}

int main(int argc, char * argv[]) {
	setvbuf(stdout, NULL, _IONBF, 0);
	struct message * test = parseMsg("DATAMESSAGE HERE");
	free(test);
	return 0;
}