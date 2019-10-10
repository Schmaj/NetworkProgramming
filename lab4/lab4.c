
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pthread.h>

#define NUM_CHILD 5

struct addStruct
{
	int x;
	int y;
};

void * add(void * arg){
	struct addStruct nums = *(struct addStruct *)arg;
	return (void*)(nums.x + nums.y); 
}

int main(int argc, char ** argv){
	pthread_t children[NUM_CHILD * (NUM_CHILD - 1)];
	for (unsigned long i = 0; i < NUM_CHILD; ++i){
		for (unsigned long j = 0; j < NUM_CHILD; ++j){
			printf("Main starting thread add() for [%ld + %ld]\n", i + 1, j + 1);
		}
	}
}