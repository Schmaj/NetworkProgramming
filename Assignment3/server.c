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

int main(int argc, char * argv[]) {
	return 0;
}

/*


struct message{
	char* originID;
	char* nextID;
	char* destinationID;
	int hopLeng;
	struct hoplist* hoplst;
}

struct hoplist{
	char* id;
	struct hoplist* next;
}

"A,D\0E\0F"

int msgToStr(struct message, char** str){
	
}

struct baseStation{
	int xPos;
	int yPos;
	char* id;
	struct siteLst* closestHead;
}

// new siteLst created for each baseStation - order of closest sites, first by proximity
struct siteLst{
	char* id;
	struct siteLst* next;
}

updateSiteLst() // call that for everybody

baseStationToBaseStation()

recvMsgFromClient()

serverToClient()

parseInput()

interactWithConsole() // getting command from user




*/