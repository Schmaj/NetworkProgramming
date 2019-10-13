/* game not case sensitive:
		is dictionary case sensitive? with regards to sorting, do 
		we need to lowercase before sorting?

		are usernames case sensitive?

		bob's broadcasted guess uses lower or upper case letters
		as he used them, the string sent to everyone maintains the 
		case used by bob


Single threaded, single process application:
	we listen for current connections before new ones, and current connections
	are dealt with in a specific order
		what should that order be?
		what if someone disconnects and someone else connects between two select calls?
			order client vs accept affects number of players sent

		order clients are dealt with affects order of broadcasts sent and received

Tell user how many players upon successful username
	Same message or second message?

*/


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


// number of clients allowed
#define BACKLOG 5
#define NO_CLIENT -1
#define TIMEOUT 15
#define WELCOME_MESSAGE "Welcome to Guess the Word, please enter your username\n"
// length of largest allowed username
#define MAX_NAME 30

// structure to hold client information
struct client{
	// file descriptor referring to socket communicating with client
	int fd;
	// username chosen by client
	char* username;
};

// sends message to all clients
void broadcast(){
	return;
}

// takes in file descriptor of communicating socket, a pointer to the list of clients
// and the index of this client in that list
void respond(struct client fd, struct client* clientList, int index){
	broadcast();
	return;
}

// taken from qsort manpage example
static int
       cmpstringp(const void *p1, const void *p2)
       {
           /* The actual arguments to this function are "pointers to
              pointers to char", but strcmp(3) arguments are "pointers
              to char", hence the following cast plus dereference */

           return strcmp(* (char * const *) p1, * (char * const *) p2);
       }


int buildDictionary(char* filename, char** dictionary, int wordSize){

	// open dictionary file
	FILE* dictFile = fopen(filename, "r");

	// index in dictionary
	int n = 0;

	// loop until end of file (break statement)
	while(1){
		// allocate memory to store next word
		char* word = calloc(wordSize + 1, sizeof(char));
		// read next line into buffer word
		char* ptr = fgets(word, wordSize + 1, dictFile);

		// if ptr is null, end of file or error, either case break
		if(ptr == NULL){
			// nothing meaningful was stored in final buffer, free memory and NULL pointer
			free(word);
			word = NULL;

			break;
		}

		// remove trailing newline
		word[strlen(word) - 1] = '\0';

		dictionary[n] = word;
		n++;
	}

	// close dictionary file
	fclose(dictFile);

	// sort the strings in dictionary in alphabetical order
	qsort(dictionary, n, sizeof(char*), cmpstringp);

	return n;

}

/*
void testQueueSort(){
	char** list = calloc(3, sizeof(char*));
	char* a = "abc";
	char* b = "def";
	char* c = "ghi";

	list[0] = c;
	list[1] = a;
	list[2] = b;

	printf("pre-sort list:\n%s\n%s\n%s\n", list[0], list[1], list[2]);

	// prints ghi then abc then def

	// pointer to first element, number of elements, comparison function (cast to correct pointer type)
	qsort(list, 3, sizeof(char*), (int (*)(const void*,const void*))strcmp);

	printf("post-sort list:\n%s\n%s\n%s\n", list[0], list[1], list[2]);

	// prints abc then def then ghi

}

*/

// communicates with client to resolve username, then adds client to clients array
void addClient(int newFd, struct client* clients, int firstOpen, int numClients, int numLetters){

	// send welcome message to client
	write(newFd, WELCOME_MESSAGE, strlen(WELCOME_MESSAGE));

	// create buffer to hold username
	char* buf = calloc(MAX_NAME + 1, sizeof(char));

	// loop if invalid usernames are given
	while(1){

		// flag for invalid name, 0 means name ok, 1 means name bad
		int invalidName = 0;

		// zero out buffer
		memset(buf, 0, MAX_NAME + 1);

		// read in user response
		int bytesRead = read(newFd, buf, MAX_NAME);

		// check if username is taken
		for(int n = 0; n < BACKLOG; n++){
			// if client has a username and it is the same as the current response
			if(clients[n].username && strcmp(clients[n].username, buf) == 0){
				// send invalid username response
				char* inval = calloc(strlen("Username  is already taken, please enter a different username\n") + MAX_NAME, sizeof(char));
				sprintf(inval, "Username %s is already taken, please enter a different username\n", buf);
				write(newFd, inval, strlen(inval));
				// free allocated memory and wait for next response
				free(inval);
				invalidName = 1;
				break;

			}
		}

		// if another name is required
		if(invalidName == 1){
			continue;
		}
		// name is correct
		else{

			// send confirm username is correct and send message with number of players and size of word

			// strlen argument is the longer of the two messages that will be stored in this buffer
			char* invite = calloc(strlen("There are  player(s) playing. The secret word is  letters.\n") + MAX_NAME, sizeof(char));
			sprintf(invite, "Let's start playing, %s\n", buf);
			write(newFd, invite, strlen(invite));

			sprintf(invite, "There are %d player(s) playing. The secret word is %d letters.\n", numClients, numLetters);
			write(newFd, invite, strlen(invite));


			// free allocated memory and break out of loop since username has been resolved
			free(invite);
			invite = NULL;
			break;
		}

	
	}

	// set file descriptor and username
	clients[firstOpen].fd = newFd;
	clients[firstOpen].username = buf;

}


int main(int argc, char* argv[]){

	// check for correct number of arguments
	if(argc < 4){
		printf("Insufficient arguments\n");
		printf("Usage: %s [seed] [port] [dictionary_file] [longest_word_length]\n", argv[0]);
		return 0;
	}

	// initialize variables from arguments

	// seed for randomizing dictionary
	int seed = atoi(argv[1]);
	// port for listening socket
	int port = atoi(argv[2]);
	// name of dictionary file
	char* dictFile = calloc(strlen(argv[3])+1, sizeof(char));
	strcpy(dictFile, argv[3]);
	// length of longest word in dictionary
	int maxWordLen = atoi(argv[4]);

	char** dictionary = calloc(64, sizeof(char*));

	int num = buildDictionary(dictFile, dictionary, maxWordLen);

	free(dictFile);

	// TODO: select word
	int wordSize = strlen(dictionary[0]);

	#ifdef DEBUG_MODE
		printf("Dict is:\n");
		for(int n = 0; n < num; n++){
			printf("\t%s\n", dictionary[n]);
			free(dictionary[n]);
			dictionary[n] = NULL;
		}
		return 0;
	#endif

	// file descriptor listening for new connections
	int listenFd = socket(PF_INET, SOCK_STREAM, 0);

	if(listenFd < 0){
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
	if(bind(listenFd, (struct sockaddr*)&server, len) != 0){
		perror("bind() failed\n");
		return EXIT_FAILURE;
	}

	// set listening socket to listening state
	if(listen(listenFd, BACKLOG) != 0){
		perror("listen() failed\n");
		return EXIT_FAILURE;
	}

	// create an array to hold each connected client
	struct client clients[BACKLOG];
	// initialize each file descriptor to -1, the chosen value to represent no client
	for(int n = 0; n < BACKLOG; n++){
		clients[n].fd = NO_CLIENT;
		clients[n].username = NULL;
	}

	while(1){
		// structure to hold a set of file descriptors to watch
		fd_set rfds;

		// set rfds to include no file descriptors
		FD_ZERO(&rfds);

		// count number of clients currently connected
		int activeClients = 0;

		for(int n = 0; n < BACKLOG; n++){
			// add all existing clients to the fd_set
			if(clients[n].fd != NO_CLIENT){
				FD_SET(clients[n].fd, &rfds);
				activeClients++;
			}
		}

		// add listening socket to fd_set
		FD_SET(listenFd, &rfds);

		// structure to specify TIMEOUT second timeout on select() call
		struct timeval timeout;

		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = 0;

		// wait for activity on listening socket, or any active client
		int retval = select(activeClients + 1, &rfds, NULL, NULL, &timeout);

		if(retval == 0){
			printf("No Activity\n");
			return 0;
		}
		else if(retval == -1){
			perror("ERROR Select() failed\n");
			return EXIT_FAILURE;
		}

		// check active clients for activity
		for(int n = 0; n < BACKLOG; n++){
			// if active client and there is activity
			if(clients[n].fd != NO_CLIENT && FD_ISSET(clients[n].fd, &rfds)){
				respond(clients[n], clients, n);
			}
		}


		// if listening socket has activity accept connection
		if(FD_ISSET(listenFd, &rfds)){
			// stores index of first unused client slot
			int firstOpen = -1;

			// check that a connection is available
			for(int n = 0; n < BACKLOG; n++){
				if(clients[n].fd != NO_CLIENT){
					firstOpen = n;
					break;
				}
			}

			// if the maximum number of clients are currently connected, ignore this connection until
			// a client disconnects
			if(firstOpen == -1){
				printf("Exceeded max number of clients, ignoring connection\n");
				continue;
			}
	
			// accept connection and store file descriptor to communicating socket		
			int newFd = accept(listenFd, NULL, NULL);

			if(newFd < 0){
				perror("accept() failed\n");
				return EXIT_FAILURE;
			}

			// resolve username and add file descriptor of new client to the list of clients
			addClient(newFd, clients, firstOpen, activeClients, wordSize);

		}
	}


}
