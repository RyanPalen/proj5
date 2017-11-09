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

typedef struct{
	int totalRes;
	int usedRes;
	int resArray[10];
}Resource;

//Function prototypes defined below
int detachandremove(int id, void *shmaddr);
void sigHandler(int sig);
long convertTime(long sec, long nano);

//global variable for signal handling and breaking out of a while loop
int killStop;


int main (int argc, char *argv[]){



	//Command line switches
	while ((c = getopt(argc, argv, "hl:i:s:t:m:")) != -1){
		switch(c){
			case 'h':
				fprintf(stderr, "Usage: %s -h -l <filename> -s [integer] -t [integer] -c\n\n", argv[0]);
				fprintf(stderr, "  %s -h\t\t Brings up this help\n", argv[0]);
				fprintf(stderr, "  %s -l <filename>\tSelects output file to save a log file\n", argv[0]);
				fprintf(stderr, "  %s -i [long]\tSets the incriment value in nanoseconds\n", argv[0]);
				fprintf(stderr, "  \t (default increment is random each interval)\n\n");
				fprintf(stderr, "  %s -s [integer]\tOverwrites the process count\n", argv[0]);
				fprintf(stderr, "  \t (default process count is %i)\n\n", maxProc);
				fprintf(stderr, "  %s -t [integer]\tOverwrites the max run time\n", argv[0]);
				fprintf(stderr, "  \t (default max time is 20 seconds)\n\n");
				fprintf(stderr, "  %s -m [long]\tOverwrites the max simulated time\n", argv[0]);
				fprintf(stderr, "  \t (default max time is 30 simulated seconds)\n\n");
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
				logFlag = 1;
				
				strftime (timeBuff, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&logTime));
				file = fopen(fileName, "a");
				fprintf(file, "----  Runtime Start: %s  ----\n", timeBuff);				
				break;
			
			//Sets the incriment value for nanosecond incriment in simulation
			case 'i':
				incVal = atol(optarg);
				incOverride = 1;
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
	
	for (i = 0; i < maxProc; i++){
		timeList[i] = temp + (rand() % (2000000000 + 1 - 0) + 0);
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

	clockKey = ftok("ftok_clock", 13);
	if (clockKey == -1){
		perror("Clock: Failed to load ftok file");
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