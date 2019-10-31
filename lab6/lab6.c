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
	while (iterator != NULL){
		printf("Not NULL!\n");

		int family = iterator->ai_family;
		char dest[128];
		struct in_addr v4addr;
		struct in6_addr v6addr;
		void* addrPtr = NULL; 
		if(family == AF_INET){
			struct sockaddr_in* info = (struct sockaddr_in*)iterator->ai_addr;
			v4addr = info->sin_addr;
			addrPtr = &v4addr;
		} 
		else if(family == AF_INET6){
			struct sockaddr_in6* info = (struct sockaddr_in6*)iterator->ai_addr;
			v6addr = info->sin6_addr;
			addrPtr = &v6addr;
		}
		else{
			printf("Unrecognized family\n");
		}

		printf("%s\n", inet_ntop(family, addrPtr, dest, 128));

		iterator = iterator->ai_next;
	}
	printf("Not NULL!\n");

	freeaddrinfo(servinfo);
	return 0;
}