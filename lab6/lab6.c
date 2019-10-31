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

int main(int argc, char * argv[]) {

	if (argc != 2){
		printf("Usage: ./a.out <hostname>\n");
		return 1;
	}

	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset (&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((status = getaddrinfo(argv[1], NULL, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo error code: %s\n", gai_strerror(status));
		return 1;
	}

	struct addrinfo * iterator = servinfo;
	while (iterator->ai_next != NULL){
		printf("Not NULL!\n");
		iterator = iterator->ai_next;
	}
	printf("Not NULL!\n");

	freeaddrinfo(servinfo);
	return 0;
}