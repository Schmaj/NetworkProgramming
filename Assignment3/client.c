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

select() to read from stdin and any tcp connections

sendMsg()

recvMsg()

interactWithConsole() // interpreting commands


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

parseMsg()

msgToStr()



*/