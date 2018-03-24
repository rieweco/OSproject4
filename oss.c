#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <getopt.h>
#include <errno.h>
#include "node.h"
#include <math.h>
#include <signal.h>
#include <limits.h>
#include <sys/msg.h>
#include <sys/wait.h>


#define MAX_LINE 100
#define DEFAULT_SLAVE 2
#define DEFAULT_RUNTIME 20
#define DEFAULT_FILENAME "logfile"
#define MAX_PR_COUNT 100

//quantum definitions
#define RR_QUANTUM 500000
#define Q1_QUANTUM 1000000
#define Q2_QUANTUM 2000000
#define Q3_QUANTUM 4000000


int numberOfSlaveProcesses;
ProcessControlBlock *pcb;
Clock *sharedClock;

//function declarations
int getNextScheduled();
pid_t r_wait(int* stat_loc);
void helpOptionPrint();
void programRunSettingsPrint(char *filename, int runtime);
void int_Handler(int);
struct Queue* generateQueue(unsigned capacity);
int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, int item);
int dequeue(struct Queue* queue);
int front(struct Queue* queue);

int main(int argc, char *argv[])
{
        //set up signal handler
        signal(SIGINT, int_Handler);

        //declare vars
        int clockMemoryID;
	int pcbMemoryID;
	//ProcessControlBlock *pcb;	
	//Clock *sharedClock;
        int total = 0;
        int opt = 0;
        //int numberOfSlaveProcesses = DEFAULT_SLAVE;
        char *filename = DEFAULT_FILENAME;
        int runtime = DEFAULT_RUNTIME;

        //read command line options
        while((opt = getopt(argc, argv, "h l:t:")) != -1)
        {
                switch(opt)
                {
                        case 'h': printf("option h pressed\n");
                                helpOptionPrint();
                                break;
                        case 'l': filename = argv[2];
                                fprintf(stderr, "Log file name set to: %s\n", filename);
                                break;
                        case 't': runtime = atoi(argv[2]);
                                fprintf(stderr, "Max runtime set to: %d\n", runtime);
                                break;
                        default: perror("Incorrect Argument! Type -h for help!");
                                exit(EXIT_FAILURE);
                }

        }
	
	//create queues
	Queue* rrQueue = generateQueue(18);
	Queue* p1Queue = generateQueue(18);
	Queue* p2Queue = generateQueue(18);
	Queue* p3Queue = generateQueue(18);
	Queue* blockQueue = generateQueue(18);	
	
	printf("queues created successfully!\n");
	

   	//print out prg settings
    	programRunSettingsPrint(filename,runtime);

    	//set up SIGALARM handler
   	if(signal(SIGALRM, int_Handler) == SIG_ERR)
	{
		perror("SINGAL ERROR: SIGALRM failed catch!\n");
		
		exit(errno);	
	}
	printf("set up sigalrm\n");
		
	//set up shared memory segment for clock
	clockMemoryID = shmget(CLOCK_KEY, sizeof(Clock), IPC_CREAT | 0666);
	if(clockMemoryID < 0)
	{
		perror("Creating clock shared memory Failed!!\n");
		exit(errno);
	}
	printf("clock mem set up successfully\n");

	//attach clock 
	sharedClock = shmat(clockMemoryID, NULL, 0);
	printf("clock attached successfully\n");

	//set up shared memory segment for pcb
	pcbMemoryID = shmget(PCB_KEY, (sizeof(ProcessControlBlock) * 18), IPC_CREAT | 0555);
	if(pcbMemoryID < 0)
	{
		perror("Creating PCB shared memory Failed!!\n");
	}
	printf("pcb memory set up successfully\n");
	
	//attach pcb
	pcb = shmat(pcbMemoryID, NULL, 0);
	printf("pcb attached successfully\n");
	
	numberOfSlaveProcesses = 0; 
        int count = 0;
        
	if(count < 2)
	{
		int totaltime = 0;
		
		//int time = ((clock->seconds * 1000000000) + clock->nano);
		int nextTime = getNextScheduled();
		
		if(totaltime >= nextTime && numberOfSlaveProcesses < 18)
		{	
			//rolls for time and queue
			srand(time(NULL));
			int priority = rand() % 10 + 1;
			int blockRoll = rand() % 10 + 1;
			int startRoll = rand() % 100000 + 1;
			int runtimeRoll = rand() % 10000000 + 1;
			int queuePlacement;
			int isBlockable;
			char blockable;

			//queue placement: roll 10 = RR Queue, else priority Queue 1
			if(priority <10)
			{
				queuePlacement = 1;
			}			
			else 
			{
				queuePlacement = 0;
			}
			
			//block roll: roll 7-10 to be blocked, else not blocked
			if(blockRoll < 7)
			{
				isBlockable = 0;
				blockable = 'N';
			}
			else 
			{
				isBlockable = 1;
				blockable = 'Y';
			}			
			
			fprintf(stderr, "Pre Fork():: queue: %d, blockable: %c, start: %d, runtime: %d\n",queuePlacement, blockable, startRoll, runtimeRoll);

			//fork slave process
			for(i = 0; i < 18; i++)
			{
				if(pcb[i].slaveID == -1)
				{
					count++;
					numberOfSlaveProcesses++;
					pcb[i].slaveNumber = i;
					pcb[i].slaveID = fork();
					
					if(pcb[i].slaveID < 0)
					{
						perror("Failed to fork90 slave process!\n");
						exit(errno);
					}
					else
					{
						pcb[i].priority = queuePlacement;
						pcb[i].isBlocked = isBlockable;
						fprintf(stderr,"priority: %d, isBlockable: %d\n", pcb[i].priority, pcb[i].isBlocked);
					}
			
				}
				else
				{
					perror("No Free Spaces in PCB!\n");
					exit(error);
				}
			}
				
		}
	}
	
	int i;
	for(i = 0; i < numberOfClaveProcesses; i++)
	{
		kill(pcb[i].slaveID, SIGINT);
	}
	
	while(wait(&wait_status) > 0)
	{
		;
	}

	free(pcb);
	detachAndRemove(

/*	

	//loop to spawn processes
        for(i; i < numberOfSlaveProcesses; i++)
        {

                fprintf(stderr,"Count: %d\n", total);
                prCount++;
		master = fork();


                if(master < 0)
                {
                        perror("Program failed to fork");
                        return 1;
                }
                else if(master > 0)
                {
                        total++;
                }
                else
                {
                        fprintf(stderr, "Process ID:%ld Parent ID:%ld slave ID:%ld\n",
                        (long)getpid(), (long)getppid(), (long)master);

                        exit(0);
                }
        }


        //while loop to check child processes and close them
        while(1)
        {
                master = wait(NULL);

                if((master == -1) && (errno != EINTR))
                {
                        break;
                }
        }

        //wait until processes are finished
        //while(r_wait(NULL) < 0);

        printf("Program terminating...\n");
        fprintf(stderr, "Total Children: %d\n", total);
*/
        return 0;
}


//function to get time for next scheduled process
int getNextScheduled()
{
	int time = 0;
	

	return time;
}

//function for exiting on Ctrl-C
void int_Handler(int sig)
{
       	signal(sig, SIG_IGN);
        printf("Program terminated using Ctrl-C\n");
        exit(0);
}

//alarm function
void alarm_Handler(int sig)
{
	int i;
	printf("Alarm! Time is UP!\n");
	for(i = 0; i < numberOfSlaveProcesses; i++)
	{
		kill(pcb[i].childID, SIGINT);	
	}
	
//	while(wait(
}

//function to wait - from UNIX book
pid_t r_wait(int* stat_loc)
{
        int retval;

        while(((retval = wait(stat_loc)) == -1) && (errno == EINTR));
        return retval;
}

//program run settings display function
void programRunSettingsPrint(char *filename, int runtime)
{
        printf("Program Run Settings:\n"); 
        fprintf(stderr,"       Log File Name:          %s\n", filename);
        fprintf(stderr,"       Max Run Time:           %d\n", runtime);
}

//function to print when -h option is used
void helpOptionPrint()
{
        printf("program help:\n"); 
        printf("        use option [-l filename] to set the filename for the logfile(where filename is the name of the logfile).\n");
        printf("        use option [-t z] to set the max time the program will run before terminating all processes (where z is the time in seconds, default: 20 seconds).\n");
        printf("        NOTE: PRESS CTRL-C TO TERMINATE PROGRAM ANYTIME.\n");
        exit(0);
}

//generate queue
struct Queue* generateQueue(unsigned capacity)
{
        struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue));
        queue->capacity = capacity;
        queue->front = queue->size = 0;
        queue->rear = capacity - 1;
        queue->array = (int*)malloc(queue->capacity * sizeof(int));
        return queue;
}

//queue is full
int isFull(struct Queue* queue)
{
        return (queue->size == queue->capacity);
}

//queue is empty
int isEmpty(struct Queue* queue)
{
        return(queue->size == 0);
}

//add item to queue
void enqueue(struct Queue* queue, int item)
{
        if(isFull(queue))
        {
                return;
        }
        else
        {
                queue->rear = (queue->rear+1)%queue->capacity;
                queue->array[queue->rear] = item;
                queue->size = queue->size + 1;
                printf("%d added to queue\n", item);
        }
}

//remove item from queue
int dequeue(struct Queue* queue)
{
        if(isEmpty(queue))
        {
                return INT_MIN;
        }
        else
        {
                int item = queue->array[queue->front];
                queue->front = (queue->front + 1)%queue->capacity;
                queue->size = queue->size - 1;
                return item;
        }
}

//get to front of queue
int front(struct Queue* queue)
{
        if(isEmpty(queue))
        {
                return INT_MIN;
        }
        else
        {
                return queue->array[queue->front];
        }
}






