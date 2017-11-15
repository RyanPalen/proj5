#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<math.h>
#include<signal.h>
#include<sys/shm.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>

#define BOUND 250000
#define TERM 250000


//Function prototypes, defined below
void sigHandler(int sig);
long convertTime(long sec, long nano);

//global variable for signal handling and breaking out of a while loop
int killStop;

typedef struct{
	int totalRes;
	int usedRes;
	int resArray[20]; //-1 in this means system has resource
	int reqList[20];
}Resource;

typedef struct{
	int turn;
	int turnAck;
	int grantedRes[20];
}Turn;

sem_t *sem;

int main (int argc, char *argv[]){
	//Iterators
	int i = 0, j = 0;
	
	//Flags
	int done = 0;
	
	//Semaphore
	sem = sem_open("/mysemaphore", 0);
	
	

	//Shared memory variables
	long *clockVar;
	Resource *resources;
	Turn *turn;
	int *procCount;
	int *pidList;

	//Shared memory keys
	key_t clockKey;
	key_t resKey;
	key_t turnKey;
	key_t countKey;
	key_t pidKey;
	
	//Shared memory IDs
	int clockID = 0;
	int resID = 0;
	int turnID = 0;
	int countID = 0;
	int pidID = 0;
	
	//random seed
	srand(time(NULL) + getpid());
	
	//Arguments from exec
	int localPID = atoi(argv[0]);
	int resAvailable = atoi(argv[1]);
	
	//Misc variables
	int resOwned[resAvailable];
	long startTime = 0;
	long timeList[50];
	long termList[50];
	int resReturn = -1;
	int resRequest = -1;
	int resCount = 0;
	
	for (i = 0; i < resAvailable; i++){
		resOwned[i] = 0;
	}
	
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
	
	countKey = ftok("ftok_count", 17);
	if (countKey == -1){
		perror("Count: Failed to load ftok file");
		return 1;
	}
	
	pidKey = ftok("ftok_pid", 17);
	if (pidKey == -1){
		perror("PID: Failed to load ftok file");
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
	
	//Initializing shared memory for the process count array
	countID = shmget(countKey, sizeof(int), 0666);
	if (countID == -1){
		perror("Count: Failed to designate shared memory");
		return 1;
	}

	procCount = shmat(countID, NULL, 0);
	if (procCount == (int*)-1){
		perror("Count: Failed to attach shared memory");
		return 1;
	}
	
	//Initializing shared memory for the process count array
	pidID = shmget(pidKey, sizeof(int[20]), 0666);
	if (countID == -1){
		perror("PID: Failed to designate shared memory");
		return 1;
	}

	pidList = shmat(pidID, NULL, 0);
	if (pidList == (int*)-1){
		perror("PID: Failed to attach shared memory");
		return 1;
	}

	/*--------------------------------------------------
	----------------------------------------------------
	---------    End shared memory section    ----------
	----------------------------------------------------
	--------------------------------------------------*/
	
	//times processes to start between 1 and 500 milliseconds
	startTime = convertTime(clockVar[0], clockVar[1]);
	long temp = startTime;
	
	for (i = 0; i < 50; i++){
		termList[i] = temp + (rand() % (TERM + 1 - 1) + 1);
		temp = termList[i];
	}
	
	long temp = startTime;
	
	for (i = 0; i < 50; i++){
		timeList[i] = temp + (rand() % (BOUND + 1 - 1) + 1);
		temp = timeList[i];
	}
		
	pidList[localPID] = getpid();
	
	int timeCount = 0;
	int termCount = 0;
	
	while(!killStop && !done){
		
		if (timeList[timeCount] < convertTime(clockVar[0], clockVar[1])){
			if ((rand() % (3 + 1 - 0) + 0) == 0 && resCount > 0){
				while (resReturn == -1 || resOwned[resReturn] < 1){
					resReturn = rand() % (resAvailable + 1 - 0) + 0;
				}
				sem_wait(sem);
				resources[resReturn].resArray[localPID]--;
				resources[i].usedRes--;
				resCount--;
				sem_post(sem);
			}
			else{
				resRequest = rand() % (resAvailable + 1 - 0) + 0;
				sem_wait(sem);
				resources[resRequest].reqList[localPID]++;
				resCount++;
				sem_post(sem);
			}
			timeCount++;
		}
		
		if (turn.turn == localPID){
			turn.turnAck = localPID;
			if (turn.turnAck == localPID){
				sem_wait(sem);
				if (resources[turn.grantedRes[localPID]].usedRes < 10){
					resources[turn.grantedRes[localPID]].resArray[localPID]++;
					
				}
				sem_post(sem);
			}
		}
		
		if (termList[termCount] < convertTime(clockVar[0], clockVar[1])){
			if ((rand() % (10 + 1 - 0) + 0) == 0){
				done = 1;
				
				sem_wait(sem);
				for (i = 0; i < resAvailable; i++){
					resources[i].resArray[localPID] -= resOwned[i];
					resources[i].usedRes - = resOwned[i];
					resources[i].reqList[localPID] = 0;
					resCount = 0;
				}
				sem_post(sem);
			}
			termCount++;
		}
		
	}
	
	
	
	
	
	//Cleanup
	procCount[0]--;
	pidList[localPID] = -1;
	return 0;


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
