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

#include <ctype.h>


// number of clients allowed
#define BACKLOG 5
#define NO_CLIENT -1
#define TIMEOUT 15
#define WELCOME_MESSAGE "Welcome to Guess the Word, please enter your username."
// length of largest allowed username
#define MAX_NAME 30

#define GUESS_DICT_LEN 499329

// structure to hold client information
struct client{
	// file descriptor referring to socket communicating with client
	int fd;
	// username chosen by client
	char* username;
};

// compares strings s1 and s2 of length len when all characters are set to lowercase
// returns 0 if words match, 1 if they do not
int caselessCmp(char* s1, char* s2){

	int len = strlen(s1);
	// if strings are different lengths, return 1 (different)
	if(strlen(s2) != len){
		return 1;
	} 

	for(int n = 0; n < len; n++){
		// if letters do not match
		if(tolower(s1[n]) != tolower(s2[n])){
			return 1;
		}
	}

	// words match
	return 0;

}

// sends message to all clients
void broadcast(struct client* clientList, char * message){
	for (unsigned int i = 0; i < BACKLOG; ++i){
		if (clientList[i].fd == -1){
			continue;
		}
		write(clientList[i].fd, message, strlen(message));
	}
	return;
}

//compares guess to secret word, 
//saving the number of letters correct and the locations correct in the variables
//guess, and secretWord MUST be NULL terminated and the same length
void compareWord(unsigned int * lettersCorrect, unsigned int * placesCorrect, char * guess, char * secretWord){
	*lettersCorrect = 0;
	*placesCorrect = 0;
	char mutableGuess[strlen(guess)+1];
	memset(mutableGuess, 0, strlen(guess)+1);
	strncpy(mutableGuess, guess, strlen(guess)+1);
	unsigned int length = strlen(secretWord);

	for (int i = 0; i < length; ++i){
		for (int n = 0; n < length; ++n){

			if (tolower(secretWord[i]) == tolower(mutableGuess[n])){

				//same location
				if (i == n){ (*placesCorrect)++; }

				//replace matching letter with symbol, reset n, go to next letter, increment
				mutableGuess[n] = '0';
				n = -1;
				i++;
				(*lettersCorrect)++;
			}
		}
	}

	return;
}

// takes in file descriptor of communicating socket, a pointer to the list of clients
// and the index of this client in that list
int respond(struct client client, struct client* clientList, 
		unsigned int max_word_length, char* secretWord){

	char message[max_word_length+1];
	memset(message, 0, max_word_length+1);

	int messageLength = read(client.fd, message, max_word_length+1);
	if (messageLength < 0){//ERROR
		perror("respond, messageLength");
	} 
	// tcp connection closed, remove client
	else if(messageLength == 0){
		// loop through clients to find disconnected client
		for(int n = 0; n < BACKLOG; n++){
			if(clientList[n].fd == client.fd){
				// close socket, free memory, and reset array index
				close(client.fd);
				clientList[n].fd = NO_CLIENT;
				if(clientList[n].username){
					free(clientList[n].username);
					clientList[n].username = NULL;
				}
				break;
			}
		}

		return 0;

	} else if (messageLength != strlen(secretWord)+1
			|| (messageLength == max_word_length+1 && message[messageLength-1] != '\n')) { //WRONG LENGTH
		
		char response[64 + strlen(secretWord)];
		memset(response, 0, 64 + strlen(secretWord));
		sprintf(response, "Invalid guess length. The secret word is %ld letter(s).", strlen(secretWord));
		write(client.fd, response, strlen(response));
		return -1;
	}

	message[messageLength-1] = '\0';

	unsigned int lettersCorrect, placesCorrect;
	compareWord(&lettersCorrect, &placesCorrect, message, secretWord);

	char response[2048];
	memset(response, 0, 2048);
	
	if (placesCorrect == strlen(secretWord)){ //CORRECT
		sprintf(response, "%s has correctly guessed the word %s", client.username, secretWord);
		broadcast(clientList, response);
		return 1;
	} else {
		sprintf(response, "%s guessed %s: %d letter(s) were correct and %d letter(s) were correctly placed.",
			client.username, message, lettersCorrect, placesCorrect);
		broadcast(clientList, response);
		return 0;
	}

	
}


// takes in name of dictionary file, a pointer to the dictionary to be filled, and the maximum size of each word
int buildDictionary(char* filename, char*** dictionaryPtr, int wordSize){

	char** dictionary = *dictionaryPtr;

	// open dictionary file
	FILE* dictFile = fopen(filename, "r");

	int size = GUESS_DICT_LEN;

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

		// if index surpasses that of our buffer to hold dictionary, expand the buffer
		if(n == size){
			// double size of array
			size *= 2;

			dictionary = realloc(dictionary, size * sizeof(char*));
			if(dictionary == NULL){
				perror("reallocarray() failed\n");
				exit(EXIT_FAILURE);
			}
		}
	}

	// close dictionary file
	fclose(dictFile);

	*dictionaryPtr = dictionary;

	return n;

}

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
		if (bytesRead < 0){
			perror("addClient, bytesRead");
		}

		buf[bytesRead-1] = '\0';

		// check if username is taken
		for(int n = 0; n < BACKLOG; n++){
			// if client has a username and it is the same as the current response
			if(clients[n].username && caselessCmp(clients[n].username, buf) == 0){
				// send invalid username response
				char* inval = calloc(strlen("Username  is already taken, please enter a different username.") + MAX_NAME, sizeof(char));
				sprintf(inval, "Username %s is already taken, please enter a different username.", buf);
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
			char* invite = calloc(strlen("There are  player(s) playing. The secret word is letter(s).") + MAX_NAME, sizeof(char));
			sprintf(invite, "Let's start playing, %s", buf);
			write(newFd, invite, strlen(invite));
			sprintf(invite, "There are %d player(s) playing. The secret word is %d letters.", numClients+1, numLetters);
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

// disconnects all clients and frees resources to prepare for next round
void disconnectClients(struct client* clients){

	for(int n = 0; n < BACKLOG; n++){
		// if a client is present
		if(clients[n].fd != NO_CLIENT){
			// close socket
			close(clients[n].fd);
			clients[n].fd = NO_CLIENT;

			// if client has a name, free memory
			if(clients[n].username){
				free(clients[n].username);
				clients[n].username = NULL;
			}
		}
	}

	return;
}

int playGame(struct client* clients, int listenFd, char* secretWord, int wordSize){

	#ifdef DEBUG_2
		printf("Listening fd is %d\n", listenFd);
	#endif

	// structure to hold a set of file descriptors to watch
	fd_set rfds;


	while(1){

		// set rfds to include no file descriptors
		FD_ZERO(&rfds);

		int maxFd = listenFd;

		// count number of clients currently connected
		int activeClients = 0;

		for(int n = 0; n < BACKLOG; n++){
			// add all existing clients to the fd_set
			if(clients[n].fd != NO_CLIENT){
				FD_SET(clients[n].fd, &rfds);
				activeClients++;

				if(clients[n].fd > maxFd){
					maxFd = clients[n].fd;
				}
			}
		}

		#ifdef DEBUG_2
			printf("Active clients is: %d\n", activeClients);
		#endif

		// add listening socket to fd_set
		FD_SET(listenFd, &rfds);

		// structure to specify TIMEOUT second timeout on select() call
		struct timeval timeout;

		timeout.tv_sec = TIMEOUT;
		timeout.tv_usec = 0;

		// wait for activity on listening socket, or any active client
		int retval = select(maxFd + 1, &rfds, NULL, NULL, &timeout);

		if(retval == 0){
			printf("No Activity\n");
			continue;
			//return 0;
		}
		else if(retval == -1){
			perror("ERROR Select() failed\n");
			return EXIT_FAILURE;
		}

		// check active clients for activity
		for(int n = 0; n < BACKLOG; n++){
			// if active client and there is activity
			if(clients[n].fd != NO_CLIENT && FD_ISSET(clients[n].fd, &rfds)){
				// respond to client's guess, if client guesses correctly (return value of 1), disconnect clients and start new game
				if(respond(clients[n], clients, wordSize, secretWord) == 1){
					disconnectClients(clients);

					//free();
					
					// end this round of the game
					#ifdef DEBUG_2
						printf("Ending game");
					#endif
					return 0;
				}
			}
		}


		// if listening socket has activity accept connection
		if(FD_ISSET(listenFd, &rfds)){
			#ifdef DEBUG_2
				printf("listenfd activity\n");
			#endif
			// stores index of first unused client slot
			int firstOpen = -1;

			// check that a connection is available
			for(int n = 0; n < BACKLOG; n++){
				if(clients[n].fd == NO_CLIENT){
					firstOpen = n;
					break;
				}
			}

			// if the maximum number of clients are currently connected, ignore this connection until
			// a client disconnects
			if(firstOpen == -1){
				printf("Exceeded max number of clients, ignoring connection");
				continue;
			}
	
			// accept connection and store file descriptor to communicating socket		
			int newFd = accept(listenFd, NULL, NULL);

			#ifdef DEBUG_2
				printf("Accepted connection\n");
			#endif

			if(newFd < 0){
				perror("accept() failed\n");
				return EXIT_FAILURE;
			}

			// resolve username and add file descriptor of new client to the list of clients
			addClient(newFd, clients, firstOpen, activeClients, wordSize);

		}
	}
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

	char** dictionary = calloc(GUESS_DICT_LEN, sizeof(char*));

	// create dictionary and return number of words
	int num = buildDictionary(dictFile, &dictionary, maxWordLen);

	free(dictFile);

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

	#ifdef DEBUG_2
		printf("Listening fd is %d\n", listenFd);
	#endif

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

	// seed the pseudo random number generator to select a word
	srand(seed);

	// loop for new rounds
	while(1){

		// select random word in dictionary and save its index
		int wordIndex = rand() % num;

		// number of letters in chosen word
		int wordSize = strlen(dictionary[wordIndex]);

		// create buffer and store secret word
		char* secretWord = calloc(wordSize + 1, sizeof(char));
		strncpy(secretWord, dictionary[wordIndex], wordSize + 1);

		// create an array to hold each connected client
		struct client clients[BACKLOG];
		// initialize each file descriptor to -1, the chosen value to represent no client
		for(int n = 0; n < BACKLOG; n++){
			clients[n].fd = NO_CLIENT;
			clients[n].username = NULL;
		}

		#ifdef DEBUG_2
			printf("Secret word is: %s\n", secretWord);
		#endif

		// interact with client and accept guesses until word is found
		playGame(clients, listenFd, secretWord, wordSize);

		free(secretWord);

	}



}
