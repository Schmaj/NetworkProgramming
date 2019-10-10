
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

struct addStruct
{
	int x;
	int y;
};

void * add(void * arg){
	struct addStruct nums = (struct addStruct)arg;
	return (void*)(num.x + nums.y); 
}

int main(int argc, char ** argv){
	
}