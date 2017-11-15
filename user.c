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
	int reqList[20];
}Resource;

typedef struct{
	int turn;
	int turnAck;
	int grantedRes[20];
}Turn;


int main (int argc, char *argv[]){
	//Iterators
	int i = 0, j = 0;
	



	//Shared memory variables
	long *clockVar;
	Resource *resources;
	Turn *turn;

	//Shared memory keys
	key_t clockKey;
	key_t resKey;
	key_t turnKey;
	
	//Shared memory IDs
	int clockID = 0;
	int resID = 0;
	int turnID = 0;
	int msgID = 0;
	
	//Arguments from exec
	int localPID = atoi(argv[0]);
	int resAvailable = atoi(argv[1]);
	
	//signal handling using signal.h
	signal(SIGINT, sigHandler);
	signal(SIGTERM, sigHandler);
	killStop = 0;


	/*--------------------------------------------------
	----------------------------------------------------
	-----------    Shared memory section    ------------
	----------------------------------------------------
	--------------------------------------------------*/
	
	//--------------------------------------------------
	//Key Initialization
	//--------------------------------------------------

	clockKey = ftok("ftok_clock", 17);
	if (clockKey == -1){
		perror("Clock: Failed to load ftok file");
		return 1;
	}
	
	resKey = ftok("ftok_res", 19);
	if (resKey == -1){
		perror("Resource: Failed to load ftok file");
		return 1;
	}
	
	turnKey = ftok("ftok_turn", 13);
	if (turnKey == -1){
		perror("Turn: Failed to load ftok file");
		return 1;
	}
	
	
	//--------------------------------------------------
	//Shared Memory Initialization
	//--------------------------------------------------
	
	//Initializing shared memory for clock counter
	clockID = shmget(clockKey, sizeof(long[2]), 0666);
	if (clockID == -1){
		perror("Clock: Failed to designate shared memory");
		return 1;
	}
	
	clockVar = shmat(clockID, 0, 0);
	if (clockVar == (long*)-1){
		perror("Clock: Failed to attach shared memory");
		return 1;
	}

	//Initializing shared memory for resource
	resID = shmget(resKey, sizeof(Resource[20]), 0666);
	if (resID == -1){
		perror("Resource: Failed to designate shared memory");
		return 1;
	}
	
	resources = shmat(resID, 0, 0);
	if (resources == (Resource*)-1){
		perror("Resource: Failed to attach shared memory");
		return 1;
	}
	
	//Initializing shared memory for the turn counter
	turnID = shmget(turnKey, sizeof(Turn), 0666);
	if (turnID == -1){
		perror("Turn: Failed to designate shared memory");
		return 1;
	}

	turn = shmat(turnID, NULL, 0);
	if (turn == (Turn*)-1){
		perror("Turn: Failed to attach shared memory");
		return 1;
	}
	




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
