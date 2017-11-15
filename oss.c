#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<math.h>
#include<signal.h>
#include<time.h>
#include<sys/shm.h>
#include<sys/stat.h>
#include<sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>

//Function prototypes defined below
int detachandremove(int id, void *shmaddr);
int checkProcTable(int *bitProc, int size);
void sigHandler(int sig);
long convertTime(long sec, long nano);

//global variable for signal handling and breaking out of a while loop
int killStop;

typedef struct{
	int totalRes;
	int usedRes;
	int resArray[20]; //-2 in this means system has resource
	int reqList[20];
}Resource;

typedef struct{
	int turn;
	int turnAck;
	int grantedRes[20];
}Turn;

//Global semaphore variable
sem_t *sem;

int main (int argc, char *argv[]){

	//Iterators and counters
	int index = 0, i = 0, j = 0;
	char c;
	time_t startTime;
	long clockTime = 0;
	int addProc = 0;
	int bitProc[20];
	int localTurn = 0;
	int resAvailable = 0;
	
	for (i = 0; i < 20; i++){
		bitProc[i] = 0;
	}
	
	//Semaphore
	sem = sem_open("/mysemaphore", O_CREAT, S_IRUSR | S_IWUSR, 0);
	
	//Flags
	int doneFlag = 0;
	int tableFull = 0;
	
	//Sig handling variables
	int status = 0;
	int killStatus = 0;
	
	//Misc variables
	pid_t childPID;
	char indexStr[5];
	char resStr[5];
	char timeStr[15];
	char timeBuff[100];
	time_t logTime = time(NULL);
	int openProc = 0;
	FILE *file;
	
	//Command line variables
	char fileName[30];
	int logFlag = 0;
	int verbFlag = 0;
	int maxTime = 2;
	int maxProc = 30;
	long incVal = 100;
	long maxSimTime = 60000000000;
	
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
	srand(time(NULL));
	
	//signal handling using signal.h
	signal(SIGINT, sigHandler);
	signal(SIGTERM, sigHandler);
	signal(SIGCHLD, SIG_IGN);

	//Command line switches
	while ((c = getopt(argc, argv, "hl:v:i:s:t:m:")) != -1){
		switch(c){
			case 'h':
				fprintf(stderr, "Usage: %s -h -l <filename> -s [integer] -t [integer] -c\n\n", argv[0]);
				fprintf(stderr, "  %s -h\t\t Brings up this help\n", argv[0]);
				fprintf(stderr, "  %s -l <filename>\tSelects output file to save a full log file\n", argv[0]);
				fprintf(stderr, "  %s -v <filename>\tSelects output file to save a verbose log file\n", argv[0]);
				fprintf(stderr, "  %s -i [long]\tSets the incriment value in nanoseconds\n", argv[0]);
				fprintf(stderr, "  \t (default increment is random each interval)\n\n");
				fprintf(stderr, "  %s -s [integer]\tOverwrites the process count\n", argv[0]);
				fprintf(stderr, "  \t (default process count is %i)\n\n", maxProc);
				fprintf(stderr, "  %s -t [integer]\tOverwrites the max run time\n", argv[0]);
				fprintf(stderr, "  \t (default max time is 2 seconds)\n\n");
				fprintf(stderr, "  %s -m [long]\tOverwrites the max simulated time\n", argv[0]);
				fprintf(stderr, "  \t (default max time is 60 simulated seconds)\n\n");
				return 1;
			
			case 'l':
				
				//Finds size of argument
				for (i = 0; optarg[i]!='\0'; i++){
					index = i;
				}
				
				//Copies filename over to fileName array
				for (i = 0; i <= index; i++){
					fileName[i] = optarg[i];
				}
				
				//Ends string
				fileName[index+1] = '\0';
				
				//Flags to output to log file
				if (verbFlag == 1){
					perror("Standard Log and Verbose Log cannot both be set\n");
					return 0;
				}
				logFlag = 1;
				
				strftime (timeBuff, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&logTime));
				file = fopen(fileName, "a");
				fprintf(file, "----  Runtime Start: %s  ----\n", timeBuff);				
				break;
			
			case 'v':
				//Finds size of argument
				for (i = 0; optarg[i]!='\0'; i++){
					index = i;
				}
				
				//Copies filename over to fileName array
				for (i = 0; i <= index; i++){
					fileName[i] = optarg[i];
				}
				
				//Ends string
				fileName[index+1] = '\0';
				
				//Flags to output to log file
				if (logFlag == 1){
					perror("Standard Log and Verbose Log cannot both be set\n");
					return 0;
				}
				verbFlag = 1;
				
				strftime (timeBuff, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&logTime));
				file = fopen(fileName, "a");
				fprintf(file, "----  Runtime Start: %s  ----\n", timeBuff);				
				break;
			
			//Sets the incriment value for nanosecond incriment in simulation
			case 'i':
				incVal = atol(optarg);
				break;
			
			//Sets the number of processes
			case 's':
				maxProc = atoi(optarg);
				
			//Sets the maxTime from the -t switch
			case 't':
				maxTime = atoi(optarg);
				break;
			
			//Sets the maxSimTime from the -m switch
			case 'm':
				maxSimTime = atol(optarg);
				break;
		}
	}
	
	//Code which creates a list of times at which to create new processes
	//This will pull from the command switch for max number of processes
	long timeList[maxProc];
	long temp = 0;
	
	//times processes to start between 1 and 500 milliseconds
	for (i = 0; i < maxProc; i++){
		timeList[i] = temp + (rand() % (500000000 + 1 - 1) + 1);
		temp = timeList[i];
	}
	 
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
	clockID = shmget(clockKey, sizeof(long[2]), IPC_CREAT | 0666);
	if (clockID == -1){
		perror("Clock: Failed to designate shared memory");
		return 1;
	}
	
	clockVar = shmat(clockID, 0, 0);
	if (clockVar == (long*)-1){
		perror("Clock: Failed to attach shared memory");
		detachandremove(clockID, clockVar);
		return 1;
	}

	//Initializing shared memory for resource
	resID = shmget(resKey, sizeof(Resource[20]), IPC_CREAT | 0666);
	if (resID == -1){
		perror("Resource: Failed to designate shared memory");
		detachandremove(clockID, clockVar);
		return 1;
	}
	
	resources = shmat(resID, 0, 0);
	if (resources == (Resource*)-1){
		perror("Resource: Failed to attach shared memory");
		detachandremove(clockID, clockVar);
		detachandremove(resID, resources);
		return 1;
	}
	
	//Initializing shared memory for the turn counter
	turnID = shmget(turnKey, sizeof(Turn), IPC_CREAT | 0666);
	if (turnID == -1){
		perror("Turn: Failed to designate shared memory");
		detachandremove(clockID, clockVar);
		detachandremove(resID, resources);
		return 1;
	}

	turn = shmat(turnID, NULL, 0);
	if (turn == (Turn*)-1){
		detachandremove(clockID, clockVar);
		detachandremove(resID, resources);		
		detachandremove(turnID, turn);
		perror("Turn: Failed to attach shared memory");
		return 1;
	}
	
	//Initializing shared memory for the process count array
	countID = shmget(countKey, sizeof(int), IPC_CREAT | 0666);
	if (countID == -1){
		perror("Count: Failed to designate shared memory");
		detachandremove(clockID, clockVar);
		detachandremove(resID, resources);		
		detachandremove(turnID, turn);
		return 1;
	}

	procCount = shmat(countID, NULL, 0);
	if (procCount == (int*)-1){
		perror("Count: Failed to attach shared memory");
		detachandremove(clockID, clockVar);
		detachandremove(resID, resources);		
		detachandremove(turnID, turn);
		detachandremove(countID, procCount);
		return 1;
	}
	
	//Initializing shared memory for the process count array
	pidID = shmget(pidKey, sizeof(int[20]), IPC_CREAT | 0666);
	if (countID == -1){
		perror("PID: Failed to designate shared memory");
		detachandremove(clockID, clockVar);
		detachandremove(resID, resources);		
		detachandremove(turnID, turn);
		detachandremove(countID, procCount);
		return 1;
	}

	pidList = shmat(pidID, NULL, 0);
	if (pidList == (int*)-1){
		perror("PID: Failed to attach shared memory");
		detachandremove(clockID, clockVar);
		detachandremove(resID, resources);		
		detachandremove(turnID, turn);
		detachandremove(countID, procCount);
		detachandremove(pidID, pidList);
		return 1;
	}
	
	/*--------------------------------------------------
	----------------------------------------------------
	---------    End shared memory section    ----------
	----------------------------------------------------
	--------------------------------------------------*/
	
	//Initialization of shared memory
	clockVar[0] = 0;
	clockVar[1] = 0;
	
	turn[0].turn = -1;
	turn[0].turnAck = -1;
	for (i = 0; i < 20; i++){
		turn[0].grantedRes[i] = -1;
	}
	
	resAvailable = rand() % (5 + 1 - 3) + 3;
	int tempRes = resAvailable;
	fprintf(stderr, "Shared Resources = %i\n\n", resAvailable);
		
	for (i = 0; i < 20; i++){
		resources[i].totalRes = 10;
		
		if (tempRes > 0){
			resources[i].usedRes = 0;
			for (j = 0; j < 20; j++){
				resources[i].resArray[j] = -1;
				resources[i].reqList[j] = 0;
			}
			tempRes--;
		}
		else{
			resources[i].usedRes = 10;
			for (j = 0; j < 20; j++){
				resources[i].resArray[j] = -2;
				resources[i].reqList[j] = 0;
			}
		}
	}
	
	procCount[0] = 0;
	
	fprintf(stderr, "Available Resources\n\n");
	for (i = 0; i < resAvailable; i++){
		fprintf(stderr, "\tR%i", i);
	}
	fprintf(stderr, "\nAv");
	for (i = 0; i < resAvailable; i++){
		fprintf(stderr, "\t%i", 10-resources[i].usedRes);
	}
	fprintf(stderr, "\n\n");
	
	
	//Sets start time for real max time kill condition
	startTime = time(NULL);
	long printTime = 1000000
	
	while ((time(NULL) - startTime) < maxTime && !killStop && clockTime < maxSimTime){
		
		//Code for simulated clock
		clockTime += incVal;
			
		clockVar[0] = clockTime/1000000000;
		clockVar[1] = clockTime % 1000000000;
		
		if (clockTime > printTime){
			
			fprintf(stderr, "Available Resources\n\n");
				for (i = 0; i < resAvailable; i++){
					fprintf(stderr, "\tR%i", i);
				}
				fprintf(stderr, "\nAv");
				for (i = 0; i < resAvailable; i++){
					fprintf(stderr, "\t%i", 10-resources[i].usedRes);
				}
				fprintf(stderr, "\n\n");
			
			printTime += clockTime;
		}
		
		//Checks against the process create time list to increase addProc
		//addProc keeps track of how many processes need to be added
		//This is important in case a process using it's quantum extends
		//the time past another entry on the list, allowing to start more than one process
		if (clockTime >= timeList[localTurn]){
			addProc++;
			localTurn++;
		}
		
		//While loop to check if the table has room and if there are processes to be added
		while (!tableFull && addProc > 0){
			
			//checks to determine if the process table has open room, function below
			openProc = checkProcTable(bitProc, 20);
			
			if (openProc == -1){
				if (logFlag){
					fprintf(file, "OSS: Checked at &ld:&ld and found Process Table full\n", clockVar[0], clockVar[1]);
				}
				tableFull = 1;
			}
			
			//Forks child process if there is room in the process table
			//and if the process count does not exceed the max
			else if (procCount[0] < maxProc){
				if ((childPID = fork()) == -1){
					fprintf(file, "OSS: Failed to fork child process\n");
					perror("Failed to Fork");
					_Exit(0);
				}
				
				//Executes the user process
				if (childPID == 0){
					procCount[0]++;
					
					sprintf(indexStr, "%d", openProc);
					sprintf(resStr, "%d", resAvailable);
					
					if (execlp("user", indexStr, resStr, NULL) == -1){
						perror("Failed to exec");
						if (logFlag){
							fprintf(file, "OSS: Failed to execute user process\n");
						}
						_Exit(1);
					}
					if (logFlag){
						fprintf(file, "OSS: User process %i started at %ld:%09ld in HIGH queue\n", clockVar[0], clockVar[1]);
					}
					_Exit(1);
					
				}
				
				//Sets a flag in the bitProc for this entry being used
				bitProc[openProc] = 1;

				//Reduces addProc which keeps track of how many processes should be added
				addProc--;				
			}
			
			
			
		}
	}
	
	//Closes processes with SIGTERM using shared memory to keep track of PIDs
	for (i = 0; i < 20; i++){
		if (pidList[i] > 0){
			
			if (logFlag){
				fprintf(file, "OSS - Cleanup: Closing Process %ld\n", pidList[i]);
			}
			
			fprintf(stderr, "Closing Process: %ld\n", pidList[i]);
			
			//SIGTERM kill of process to allow cleanup
			kill(pidList[i], SIGTERM);
			
			//Checks status of process closing over the course of 10 second wait with 5 checks
			for (j = 0; j < 8; j++){
				killStatus = waitpid(pidList[i], &status, WNOHANG);

				//Process didn't close properly, error
				if (killStatus == -1){
					
					if (logFlag){
						fprintf(file, "OSS - Cleanup: Failed to kill Process %ld\n", pidList[i]);
					}
					
					perror("Kill Process Failed");
					break;
				}
				//Waiting for process to finish with 2 second sleep
				else if(killStatus == 0){
					
					fprintf(stderr, "Waiting for Process %ld to end...\n", pidList[i]);
					
					sleep(1);
				}
				//Process closed - provides information on close
				else if(killStatus == pidList[i]){
					//Process ends normally if WIFEXITED status is true
					if (WIFEXITED(status)){
						
						if (logFlag){
							fprintf(file, "OSS - Cleanup: Process %ld ended normally\n", pidList[i]);
						}
						
						fprintf(stderr, "Process %ld ended normally\n", pidList[i]);
						break;
					}
					//Process did not catch signal if WIFSIGNALED status true
					else if(WIFSIGNALED(status)){
						
						if (logFlag){
							fprintf(file, "OSS - Cleanup: Process %ld ended due to uncaught signal\n", pidList[i]);
						}
						
						fprintf(stderr, "Process %ld ended because of uncaught signal\n", pidList[i]);
						break;
					}
				}
			}
		}
	}
	
	fprintf(stderr, "Detaching Shared Memory\n");
	detachandremove(clockID, clockVar);
	detachandremove(resID, resources);		
	detachandremove(turnID, turn);
	detachandremove(countID, procCount);
	detachandremove(pidID, pidList);
	
	return 0;
}


//Cleans up shared memory using the Shared memory control function
//Complete with error checking!
int detachandremove(int id, void *shmaddr){
	int error = 0;

	if (shmdt(shmaddr) == -1){
		error = errno;
	}
	if ((shmctl(id, IPC_RMID, NULL) == -1) && !error){
		error = errno;
	}
	if (!error){
		return 0;
	}
	errno = error;
	return -1;
}


//Function to check if process table has room and return index
int checkProcTable(int *bitProc, int size){
	int i = 0;
	
	for (i = 0; i < size; i++){
		if (bitProc[i] == 0){
			bitProc[i] = 1;
			return i;
		}
	}
	return -1;
}

//Basic signal handler to check for Ctrl+C and SIGTERM sent to child processes
void sigHandler(int sig){
	if (sig == SIGINT){
		fprintf(stderr, "\nMain: Force closing processes\n");
		killStop = 1;
	}
	if (sig == SIGTERM){
		fprintf(stderr, "Found a SIGTERM!\n");
	}
}

//Time conversion to get a nanosecond representation from two longs
long convertTime(long sec, long nano){
	return ((sec*1000000000) + nano);
}