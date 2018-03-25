#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include <string.h>

#define MAX_LINE 100
#define DEFAULT_SLAVE 2
#define DEFAULT_RUNTIME 5
#define DEFAULT_FILENAME "logfile"
#define MAX_PR_COUNT 100
#define MAXTIMEBETWEENNEWPROCSNS 5000000
#define MAXTIMEBETWEENNEWPROCSSECS 1

//quantum definitions
#define RR_QUANTUM 500000
#define Q1_QUANTUM 1000000
#define Q2_QUANTUM 2000000
#define Q3_QUANTUM 4000000


int numberOfSlaveProcesses;
ProcessControlBlock *pcb;
Clock *sharedClock;
Slave *slave;
char* pcbAddress;
int spawnNewTime;
//char* sharedClockAddress;

//function declarations
Slave performRolls(pid_t id);
void printSlaveInfo(ProcessControlBlock* p);
//int getClockTime(Clock** c);
//int getNextScheduled();
pid_t r_wait(int* stat_loc);
void helpOptionPrint();
void programRunSettingsPrint(char *filename, int runtime);
void int_Handler(int);
int detachAndRemove(int shmid, void *shmaddr);
struct Queue* generateQueue(unsigned capacity);
int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, int item);
int dequeue(struct Queue* queue);
int front(struct Queue* queue);
int getSpawnTime();

int main(int argc, char *argv[])
{
        //set up signal handler
        signal(SIGINT, int_Handler);

        //declare vars
        int spawnTime;
	int spawnSeconds;
	int spawnNanoseconds;
        int clockMemoryID;
	int pcbMemoryID;
        int opt = 0;
        int wait_status;
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

	//create array with size of 18 Slave structs
	slave = (Slave *)malloc(sizeof(Slave) * 18);
	

   	//print out prg settings
    	programRunSettingsPrint(filename,runtime);

    	//set up SIGALARM handler
   	if(signal(SIGALRM, int_Handler) == SIG_ERR)
	{
		perror("SINGAL ERROR: SIGALRM failed catch!\n");
		
		exit(errno);	
	}

	alarm(runtime);
		
	//set up shared memory segment for clock
	clockMemoryID = shmget(CLOCK_KEY, sizeof(Clock), IPC_CREAT | 0666);
	if(clockMemoryID < 0)
	{
		perror("Creating clock shared memory Failed!!\n");
		exit(errno);
	}

	//attach clock 
	sharedClock = shmat(clockMemoryID, NULL, 0);

	//initialize clock
	sharedClock->seconds = 0;
	sharedClock->nanoseconds = 0;

	//set up shared memory segment for pcb
	pcbMemoryID = shmget(PCB_KEY, 2048, IPC_CREAT | 0666);
	if(pcbMemoryID < 0)
	{
		perror("Creating PCB shared memory Failed!!\n");
	}
	
	//attach pcb
	pcbAddress = shmat(pcbMemoryID, NULL, 0);

	//initialize pcb with slaveID = -1;
	pcb = (ProcessControlBlock*) ((void*)pcbAddress+sizeof(int));	
	
	int q;
	for(q = 0; q < 18; q++)
	{
		pcb[q].slaveID = -1;
	}

	
	numberOfSlaveProcesses = 0; 
        int count = 0;
        
	//rolls for time and queue
	srand(time(NULL));
	int priority = rand() % 10 + 1;
	int blockRoll = rand() % 10 + 1;
	int startRoll = rand() % 100000 + 1;
	int runtimeRoll = rand() % 10000000 + 1;
	int queuePlacement;
	int isBlockable;

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
	}
	else 
	{
		isBlockable = 1;
	}			
			
	// get spawn time
	spawnTime = getSpawnTime();
	spawnSeconds = spawnTime / 1000000000;
	spawnNanoseconds = spawnTime % 1000000000;
	printf("spawntime: %d, sec: %d, nano: %d\n",spawnTime,spawnSeconds,spawnNanoseconds);
	//sharedClock->seconds = spawnSeconds;
	//sharedClock->nanoseconds = spawnNanoseconds;

	//fork slave process
	int i;
	count++;
	numberOfSlaveProcesses++;
	for(i = 0; i < count; i++)
	{
		printf("count: %d\n",count);
		slave[0].slaveID = fork();
					
		if(slave[0].slaveID < 0)
		{
			perror("Failed to fork() slave process!\n");
			exit(errno);
		}
		else if(slave[0].slaveID == 0)
		{
			slave[0].slaveID = getpid();
			slave[0].priority = queuePlacement;
			slave[0].isBlockable = isBlockable;
			slave[0].duration = runtimeRoll;
			slave[0].progress = 0;
			slave[0].start = startRoll;
			pcb[0].slaveID = slave[0].slaveID;
			pcb[0].priority = queuePlacement;
			pcb[0].isBlocked = isBlockable;
			pcb[0].start = startRoll;
			pcb[0].hasStarted = 0;
			pcb[0].index = 0;


			execl("./slave","./slave",slave[0].slaveID,NULL);
		}
		else
		{
			
			if(slave[0].priority == 0)
			{	
				enqueue(rrQueue,slave[0].slaveID);
				printf("slaveID: %d placed in RR Queue\n",slave[0].slaveID);
			}
			else
			{
				enqueue(p1Queue,slave[0].slaveID);
				printf("slaveID: %d placed in Priority 1 Queue\n",slave[0].slaveID);	
			}
		}
		
	}

	
	while(count < 5)
	{
		int sec;
		int nano;
		sec = sharedClock->seconds * 1000000000;
		nano = sharedClock->nanoseconds;
		int currentTime = sec + nano;
		
		if(currentTime >= spawnTime && numberOfSlaveProcesses < 5)
		{
			spawnTime = getSpawnTime() + currentTime;
			count++;
			int index;
			int i;
			for(i = 0; i < 18; i++)
			{
				if(pcb[i].slaveID == -1)
				{
					index = pcb[i].index;
					break;
				}
			}
			printf("count: %d\n",count);
			slave[index].slaveID = fork();
			if(slave[index].slaveID < 0)
			{
				perror("Failed to fork() slave process!\n");
				exit(errno);
			}
			else if(slave[index].slaveID == 0)
			{
				pid_t id = getpid();
				slave[index] = performRolls(id);
						
				pcb[index].slaveID = slave[index].slaveID;
				pcb[index].priority = slave[index].priority;
				pcb[index].isBlocked = slave[index].isBlockable;
				slave[index].isBlockable = 0;
				pcb[index].start = slave[index].start;
				pcb[index].hasStarted = 0;
				pcb[index].index = index;

				printf("Slave: SlaveID: %d, spawned at %d\n", slave[index].slaveID, currentTime);
			}
		}
		else
		{
   			currentTime = currentTime + 500000;
			nano = nano + 50000000;
			if(nano > 1000000000)
			{
				sec = sec + 1000000000;
				nano = nano - 1000000000;
				sharedClock->seconds = sec/1000000000;
				sharedClock->nanoseconds = nano;
				printf("clock is now: %d.%d sec\n",sharedClock->seconds, sharedClock->nanoseconds);
			}
			else
			{
				printf("nano less than 1000000000\n");
				sharedClock->seconds = sec/1000000000;
				sharedClock->nanoseconds = nano;	
				printf("clock is now: %d.%d sec\n",sharedClock->seconds, sharedClock->nanoseconds);
			}
		}			
	}
	
	
	//print pcb
	int x;
	for(x = 0; x < 5; x++)
	{
		if(pcb[x].slaveID != -1)
		{
			printSlaveInfo(&pcb[x]);
		} 
	}
	
	//print slave
	int g;
	for(g = 0; g < 5; g++)
	{
		printf("slave: slaveID: %d\n",slave[g].slaveID);
	}
		
	
	//kill slave processes
	int j;
	for(j = 0; j < numberOfSlaveProcesses; j++)
	{
		kill(slave[j].slaveID, SIGINT);
	}
	
	//wait for slaves to finish	
	while(wait(&wait_status) > 0)
	{
		;
	}
	
	//free shared memory ****** one of these isnt working! ****
	free(slave);
	detachAndRemove(pcbMemoryID, pcb);
	detachAndRemove(clockMemoryID, sharedClock);
	

	return 0;

}


//function to perform all random rolls
Slave performRolls( pid_t id)
{
	Slave s;
	srand(time(NULL));
	int priority = rand() % 10 + 1;
	int blockRoll = rand() % 10 + 1;
	int startRoll = rand() % 100000 + 1;
	int runtimeRoll = rand() % 10000000 + 1;
	int queuePlacement;
	int isBlockable;

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
	}
	else 
	{
		isBlockable = 1;
	}
	
	s.slaveID = id;
	s.priority = priority;
	s.isBlockable = isBlockable;
	s.start = startRoll;
	s.duration = runtimeRoll;
	s.progress = 0;		
	
	return s;
}
/*

//function to get scheduled times
int getScheduledTimes(ProcessControlBlock* p)
{
	int 
	return start;
}

//function to get clock time
int getClockTime(Clock* c)
{
	int time;
	int sec;
	int nano;
	sec = c->seconds * 1000000000;
	nano - c->nanoseconds;
	time = sec + nano;
	return time;
}
*/
//function to get next spawn time
int getSpawnTime()
{
	int spawnTime;
	int sec;
	int nano;
	srand(time(NULL));

	sec = rand() % MAXTIMEBETWEENNEWPROCSSECS + 1;
	nano = rand() % MAXTIMEBETWEENNEWPROCSNS;

	sec = sec * 1000000000;
	
	spawnTime = sec + nano;

	return spawnTime;
}

//function to print slave info
void printSlaveInfo(ProcessControlBlock* p)
{
	printf("pcb[%d]:\tslaveID: %d\n", p->index, p->slaveID);
	printf("\tpriority: %d\tstart:%d\thasStarted:%d\n", p->priority, p->start, p->hasStarted);
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
		kill(slave[i].slaveID, SIGINT);	
	}
	
//	while(wait(
}

//function to detach and remove shared memory -- from UNIX Book
int detachAndRemove(int shmid, void *shmaddr)
{
	int error = 0;
	if(shmdt(shmaddr) == -1)
	{
		error = errno;	
	}
	if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error)
	{
		error = errno;
	}
	if(!error)
	{
		return 0;
	}
	errno = error;
	return -1;
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






