
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
	unsigned int x;
	unsigned int y;
};

void * add(void * arg){
	struct addStruct nums = *(struct addStruct *)arg;
	printf("Thread %ld running add() with [%d + %d]\n", pthread_self(), nums.x, nums.y);
	unsigned int answer = nums.x + nums.y;
	free(arg);
	return (void*)answer; 
}

int main(int argc, char ** argv){
	pthread_t children[NUM_CHILD * (NUM_CHILD - 1)];

	for (unsigned int i = 0; i < NUM_CHILD; ++i){
		for (unsigned int j = 0; j < NUM_CHILD; ++j){
			pthread_t tid;
			printf("Main starting thread add() for [%d + %d]\n", i + 1, j + 1);
			struct addStruct * arg = calloc(1, sizeof(struct addStruct));
			arg->x = i + 1;
			arg->y = j + 1;
			int val = pthread_create(&tid, NULL, add, (void *)arg);

			if (val < 0){
				return -1;
			} else {
				children[(i*5) + j] = tid;
			}
		}
	}

	unsigned int x, y;
	for (unsigned int i = 0; i < NUM_CHILD * (NUM_CHILD - 1); ++i){
		unsigned int *retval;
		pthread_join(children[i], (void **)&retval);
		x = i/NUM_CHILD;
		y = i/NUM_CHILD;
		printf("In main, collecting thread %ld computed [%d + %d] = %d\n", children[i], x, y, 
			(unsigned int)retval);
	}
	return 0;
}