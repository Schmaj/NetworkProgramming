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

	for (unsigned int i = 0; i < length; ++i){
		if (tolower(secretWord[i]) == tolower(guess[i])){
			(*placesCorrect)++;
		}
	}

	for (int i = 0; i < length; ++i){
		for (int n = 0; n < length; ++n){

			if (tolower(secretWord[i]) == tolower(mutableGuess[n])){

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

int main(){
 unsigned int lettersCorrect, placesCorrect;
 char * guess = "ratat";
 char * secretWord = "untap";
 compareWord(&lettersCorrect, &placesCorrect, guess, secretWord);
 printf("LETTERS: %u\n PLACES: %u\n", lettersCorrect, placesCorrect);
}