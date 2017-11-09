#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<math.h>
#include<signal.h>
#include<sys/shm.h>
#include<sys/stat.h>
#include<sys/wait.h>

//Function prototypes, defined below
void sigHandler(int sig);
long convertTime(long sec, long nano);

//global variable for signal handling and breaking out of a while loop
int killStop;

typedef struct{
	int totalRes;
	int usedRes;
	int resArray[10]; //-1 in this means system has resource
}Resource;


int main (int argc, char *argv[]){	




}


//Basic signal handler to check for Ctrl+C and SIGTERM sent to child processes
void sigHandler(int sig){
	if (sig == SIGINT){
		fprintf(stderr, "Main: Force closing processes\n");
		killStop = 1;
	}
	if (sig == SIGTERM){
		fprintf(stderr, "Child: Process %ld being closed by main\n", getpid());
		killStop = 1;
	}
}

//Time conversion to get a nanosecond representation from two longs
long convertTime(long sec, long nano){
	return ((sec*1000000000) + nano);
}
