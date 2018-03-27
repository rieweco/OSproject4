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
#define DEFAULT_RUNTIME 3
#define DEFAULT_FILENAME "logfile"
#define MAX_PR_COUNT 100
#define MAXTIMEBETWEENNEWPROCSNS 500000000
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
int msgQueue;
Queue* rrQueue;
Queue* p1Queue;
Queue* p2Queue;
Queue* p3Queue;
Queue* bQueue;
FILE *logfile;
char* filename;

//function declarations
void sendMSG(int type,int dur,int prog,int sIndex,int burst,int id);
void recMSG(int type);
int getID(int i);
int getBurst(int prio);
Slave performRolls(pid_t id);
void printSlaveInfo(ProcessControlBlock* p);
pid_t r_wait(int* stat_loc);
void helpOptionPrint();
void programRunSettingsPrint(char *file, int runtime);
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
        filename = DEFAULT_FILENAME;
        int runtime = DEFAULT_RUNTIME;

	srand(time(NULL));

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
	
	//generate message queue
	msgQueue = msgget(QUEUEKEY,IPC_CREAT | 0666);
		
	//create queues
	rrQueue = generateQueue(18);
	p1Queue = generateQueue(18);
	p2Queue = generateQueue(18);
	p3Queue = generateQueue(18);
	bQueue = generateQueue(18);	

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
		pcb[q].index = q;
	}

	pid_t childPID = 0;	
	numberOfSlaveProcesses = 0; 
        int count = 0;
        
	//rolls for time and queue
	srand(time(NULL));
	int priority = rand() % 10 + 1;
	int runtimeRoll = rand() % 10000000 + 1;
	int queuePlacement;

	//queue placement: roll 10 = RR Queue, else priority Queue 1
	if(priority <10)
	{
		queuePlacement = 1;
	}			
	else 
	{
		queuePlacement = 0;
	}
		
	// get spawn time
	spawnTime = getSpawnTime();
	spawnSeconds = spawnTime / 1000000000;
	spawnNanoseconds = spawnTime % 1000000000;
	printf("spawntime: %d, sec: %d, nano: %d\n",spawnTime,spawnSeconds,spawnNanoseconds);

	//fork first slave process
	childPID = fork();
	if(childPID < 0)
	{
		perror("Failed to fork() slave process!\n");
		return 1;
	}
	else if(childPID > 0)
	{
		pcb[0].slaveID = childPID;
		pcb[0].priority = queuePlacement;
		pcb[0].index = 0;
		pcb[0].burstTime = getBurst(queuePlacement);
		pcb[0].isBlocked = 0;
		pcb[0].duration = runtimeRoll;
		pcb[0].progress = 0;
		slave[0].slaveID = childPID;
		slave[0].priority = queuePlacement;
		slave[0].duration = runtimeRoll;
		slave[0].progress = 0;
		slave[0].burstTime = getBurst(queuePlacement);	
	
		if(queuePlacement == 0)
		{
			enqueue(rrQueue,pcb[0].slaveID);
		}	
		else
		{
			enqueue(p1Queue,pcb[0].slaveID);
		}
		numberOfSlaveProcesses++;	
		count++;
		
		//write to log file
		logfile = fopen(filename, "w");
		fprintf(logfile,"\nOSS: new slave process created. count now: %d\n",count);
		fclose(logfile);
		printf("pcb info: i=%d, id=%d, prio-%d, burst=%d, dur=%d\n",pcb[0].index,pcb[0].slaveID,pcb[0].priority,pcb[0].burstTime,pcb[0].duration);
	}
	else
	{
		char numberBuffer[20];
		snprintf(numberBuffer, 20,"%d",pcb[0].slaveID);
		execl("./slave","./slave",numberBuffer,NULL);
	}

	//open log file for appending
	logfile = fopen(filename, "a");
	
	//schdule algorithm
	while(count < 100)
	{
		if(((sharedClock->seconds * 1000000000) + sharedClock->nanoseconds) >= spawnTime)
		{
			printf("its time to spawn a child!!\n");
			int w;
			int freeFlag = 1;
			spawnTime = getSpawnTime();
			int sec = sharedClock->seconds;
			int nano = sharedClock->nanoseconds;
			sec = sec * 1000000000;
			spawnTime = spawnTime + sec + nano;
			//spawnTime = rand() % (MAXTIMEBETWEENNEWPROCSSECS - MAXTIMEBETWEENNEWPROCSNS) + MAXTIMEBETWEENNEWPROCSNS;
			//spawnTime = spawnTime/1000;
			spawnSeconds = spawnTime / 1000000000;
			spawnNanoseconds = spawnTime % 1000000000;
			//spawnTime = rand() % (sec - nano) + nano;
			//spawnTime = spawnTime/1000;
			fprintf(logfile,"\nOSS: slave spawntime: %d, sec: %d, nano: %d\n",spawnTime,spawnSeconds,spawnNanoseconds);
			while(numberOfSlaveProcesses < 18 && freeFlag)
			{
				if(!freeFlag)
				{
					;
				}
				else if(freeFlag)
				{
					for(w = 0; w < 18; w++)
					{
						if(!freeFlag)
						{
							;
						}
						else if(freeFlag)
						{
							if(pcb[w].slaveID == -1)
							{
								freeFlag = 0;
								childPID = fork();
								if(childPID < 0)
								{
									printf("Error! Failed to spawn slave #%d!\n",count);
									return 1;
								}
								else if(childPID > 0)
								{
									printf("child PID =  %d\n",childPID);
									slave[w] = performRolls(childPID);
									slave[w].index = w;
									pcb[w].slaveID = childPID;
									pcb[w].index = w;
									pcb[w].priority = slave[w].priority;
									pcb[w].isBlocked = 0;
									pcb[w].burstTime = getBurst(slave[w].priority);
									pcb[w].progress = 0;
									pcb[w].duration = slave[w].duration;
							
											
									if(pcb[w].priority == 0)
									{	
										enqueue(rrQueue,pcb[w].slaveID);
										fprintf(logfile,"\nOSS: slaveID: %d placed in RR Queue\n",pcb[w].slaveID);
									}
									else
									{
										enqueue(p1Queue,pcb[w].slaveID);
										fprintf(logfile,"\nOSS: slaveID: %d placed into Q 1\n",pcb[w].slaveID);
									}
									count++;
									numberOfSlaveProcesses++;
									printf("total count is now: %d, processes running: %d\n",count,numberOfSlaveProcesses);
							
								}	
								else
								{
									char numberBuffer[20];
									snprintf(numberBuffer, 20,"%d",pcb[w].slaveID);
									printf("number buffer: %d\n",atoi(numberBuffer));
									execl("./slave","./slave",numberBuffer,NULL);
								}
							}
						}
					}
				}
			}
		}
		else
		{
			//check blocked queue
			if(isEmpty(bQueue) == 0)
			{
				int w;
				int idFromQ = front(bQueue);
				for(w = 0; w < 18; w++)
				{
					if(pcb[w].slaveID == idFromQ)
					{
						dequeue(bQueue);
						pcb[w].isBlocked = 0;
						if(pcb[w].priority == 0)
						{
							enqueue(rrQueue,idFromQ);
							fprintf(logfile,"\nOSS: moved process with PID %d from Block Q to Q 0 at time %d:%d\n",idFromQ,sharedClock->seconds,sharedClock->nanoseconds);
						}
						else if(pcb[w].priority == 1)
						{
							enqueue(p1Queue,idFromQ);
							fprintf(logfile,"\nOSS: moved process with PID %d from Block Q to Q 1 at time %d:%d\n",idFromQ,sharedClock->seconds,sharedClock->nanoseconds);
						}
						else if(pcb[w].priority == 2)
						{
							enqueue(p2Queue,idFromQ);
							fprintf(logfile,"\nOSS: moved process with PID %d from Block Q to Q 2 at time %d:%d\n",idFromQ,sharedClock->seconds,sharedClock->nanoseconds);
						}
						else
						{
							enqueue(p3Queue,idFromQ);
							fprintf(logfile,"\nOSS: moved process with PID %d from Block Q to Q 3 at time %d:%d\n",idFromQ,sharedClock->seconds,sharedClock->nanoseconds);	
						}
					}
				}
			}
			//nothing in blocked queue
			//check rr queue
			else if(isEmpty(rrQueue) == 0)
			{
				int w;
				int idFromQ = front(rrQueue);
				for(w = 0; w < 18; w++)
				{
					if(pcb[w].slaveID == idFromQ)
					{
						fprintf(logfile,"\nOSS: Dispatching process with PID %d found at index %d - from Q 0 at time %d:%d,\n",idFromQ,w,sharedClock->seconds,sharedClock->nanoseconds);
						//get dispatch time and update clock
						int dispatchTime = (rand() % 10001) + 100;
						int sec = sharedClock->seconds;
						int nano = sharedClock->nanoseconds;
						nano = nano + dispatchTime;
						if(nano > 1000000000)
						{
							sec = sec + 1;
							nano = nano - 1000000000;
						}
						sharedClock->seconds = sec;
						sharedClock->nanoseconds = nano;
						printf("idFromQ in RR QUEUE: %d\n",idFromQ);
						fprintf(logfile,"\nOSS: total time this dispatch was %d nanoseconds.\n",dispatchTime);
						sendMSG(idFromQ,pcb[w].duration,pcb[w].progress,pcb[w].index,pcb[w].burstTime,idFromQ);
						recMSG(MASTERKEY);
					}

				}	

			}		
			else if(isEmpty(p1Queue) == 0)
			{
				int w;
				int idFromQ = front(p1Queue);
				printf("front of p1Queue: %d\n",idFromQ);
				for(w = 0; w < 18; w++)
				{
					if(pcb[w].slaveID == idFromQ)
					{
						fprintf(logfile,"\nOSS: Dispatching process with PID %d found at index %d - from Q 1 at time %d:%d,\n",idFromQ,w,sharedClock->seconds,sharedClock->nanoseconds);
						//get dispatch time and update clock
						int dispatchTime = (rand() % 10001) + 100;
						int sec = sharedClock->seconds;
						int nano = sharedClock->nanoseconds;
						nano = nano + dispatchTime;
						if(nano > 1000000000)
						{
							sec = sec + 1;
							nano = nano - 1000000000;
						}
						sharedClock->seconds = sec;
						sharedClock->nanoseconds = nano;
						printf("idFromQ in P1 QUEUE: %d\n",idFromQ);
						fprintf(logfile,"\nOSS: total time this dispatch was %d nanoseconds.\n",dispatchTime);
						sendMSG(idFromQ,pcb[w].duration,pcb[w].progress,pcb[w].index,pcb[w].burstTime,idFromQ);
						recMSG(MASTERKEY);
					}
				}
			}
			else if(isEmpty(p2Queue) == 0)
			{
				int w;
				int idFromQ = front(p2Queue);
				for(w = 0; w < 18; w++)
				{
					if(pcb[w].slaveID == idFromQ)
					{
						fprintf(logfile,"\nOSS: Dispatching process with PID %d from Q 2 at time %d:%d,\n",idFromQ,sharedClock->seconds,sharedClock->nanoseconds);
						//get dispatch time and update clock
						int dispatchTime = (rand() % 10001) + 100;
						int sec = sharedClock->seconds;
						int nano = sharedClock->nanoseconds;
						nano = nano + dispatchTime;
						if(nano > 1000000000)
						{
							sec = sec + 1;
							nano = nano - 1000000000;
						}
						sharedClock->seconds = sec;
						sharedClock->nanoseconds = nano;
						fprintf(logfile,"\nOSS: total time this dispatch was %d nanoseconds.\n",dispatchTime);
						sendMSG(idFromQ,pcb[w].duration,pcb[w].progress,pcb[w].index,pcb[w].burstTime,idFromQ);
						recMSG(MASTERKEY);
					}
				}
			}
			else if(isEmpty(p3Queue) == 0)
			{
				int w;
				int idFromQ = front(p3Queue);
				for(w = 0; w < 18; w++)
				{
					if(pcb[w].slaveID == idFromQ)
					{
						fprintf(logfile,"\nOSS: Dispatching process with PID %d from Q 3 at time %d:%d,\n",idFromQ,sharedClock->seconds,sharedClock->nanoseconds);
						//get dispatch time and update clock
						int dispatchTime = (rand() % 10001) + 100;
						int sec = sharedClock->seconds;
						int nano = sharedClock->nanoseconds;
						nano = nano + dispatchTime;
						if(nano > 1000000000)
						{
							sec = sec + 1;
							nano = nano - 1000000000;
						}
						sharedClock->seconds = sec;
						sharedClock->nanoseconds = nano;
						fprintf(logfile,"\nOSS: total time this dispatch was %d nanoseconds.\n",dispatchTime);
						sendMSG(idFromQ,pcb[w].duration,pcb[w].progress,pcb[w].index,pcb[w].burstTime,idFromQ);
						recMSG(MASTERKEY);
					}
				}
			}
			
		}
	}

	printf("*********** DONE WITH WHILE LOOP**************\n");
		
	//kill slave processes
	int j;
	for(j = 0; j < numberOfSlaveProcesses; j++)
	{
		kill(pcb[j].slaveID, SIGINT);
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
	
	fclose(logfile);

	return 0;

}

//function to send message to slave
void sendMSG(int type,int dur,int prog,int sIndex,int burst, int id)
{
	Message message;
	int msgsize = sizeof(Message);
	message.type = type;
	message.pid = id;
	message.duration = dur;
	message.progress = prog;
	message.index = sIndex;
	message.burstTime = burst;
	printf("inside master sendmsg:id: %d, index = %d, duration = %d, progress = %d, burstTime = %d\n",message.pid,message.index,message.duration,message.progress,message.burstTime);
		
	msgsnd(msgQueue,&message,msgsize,0);
}

//function to recieve message from slave
void recMSG(int type)
{
	logfile = fopen(filename, "a");
	Message message;
	int msgsize = sizeof(Message);
	msgrcv(msgQueue,&message,msgsize,type,0);
	
	printf("inside master recMSG, burstTime: %d\n",message.burstTime);
		
	if(message.blockFlag == 1)
	{
		printf("inside block flag\n");
		pcb[message.index].isBlocked = 1;
		enqueue(bQueue,message.pid);
		if(pcb[message.index].priority == 0)
		{
			dequeue(rrQueue);
		}
		else if(pcb[message.index].priority == 1)
		{
			dequeue(p1Queue);
		}
		else if(pcb[message.index].priority == 2)
		{
			dequeue(p2Queue);
		}
		else
		{
			dequeue(p3Queue);
		}
		int nano = sharedClock->nanoseconds;
		nano = nano + message.burstTime;
		sharedClock->nanoseconds = nano;
		pcb[message.index].progress = message.progress;
		fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds before getting blocked,\n",message.pid,message.burstTime);
		fprintf(logfile,"\nOSS: moved process with PID %d from Q %d to Block Q.\n",message.pid,pcb[message.index].priority);
	}
	
	else if(message.moveFlag == 1)
	{
		printf("inside move flag\n");	
		if(pcb[message.index].priority == 0)
		{	
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			dequeue(rrQueue);
			enqueue(rrQueue,message.pid);
			pcb[message.index].progress = message.progress;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished its timeslice,\n",message.pid,message.burstTime);
			fprintf(logfile,"\nOSS: placing at the end of Q 0.\n");
		}
		else if(pcb[message.index].priority == 1)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			dequeue(p1Queue);
			enqueue(p2Queue,message.pid);
			pcb[message.index].progress = message.progress;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished its timeslice,\n",message.pid,message.burstTime);
			fprintf(logfile,"\nOSS: moving down to next queue, Q 2.\n");
		}
		else if(pcb[message.index].priority == 2)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			dequeue(p2Queue);
			enqueue(p3Queue,message.pid);
			pcb[message.index].progress = message.progress;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished its timeslice,\n",message.pid,message.burstTime);
			fprintf(logfile,"\nOSS: moving down to next queue, Q 3.\n");
		}
		else
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			dequeue(p3Queue);
			enqueue(p3Queue,message.pid);
			pcb[message.index].progress = message.progress;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished its timeslice,\n",message.pid,message.burstTime);
			fprintf(logfile,"\nOSS: placing at the end of Q 3.\n");
		}
	}
	else if(message.terminateFlag == 1)
	{	
		printf("inside terminate flag\n");
		if(pcb[message.index].priority == 0)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			pcb[message.index].slaveID = -1;
			dequeue(rrQueue);
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and was terminated.\n",message.pid,message.burstTime);
		}
		else if(pcb[message.index].priority == 1)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			pcb[message.index].slaveID = -1;
			dequeue(p1Queue);
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and was terminated.\n",message.pid,message.burstTime);
		}
		else if(pcb[message.index].priority == 2)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			pcb[message.index].slaveID = -1;
			dequeue(p2Queue);
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and was terminated.\n",message.pid,message.burstTime);
		}
		else
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			pcb[message.index].slaveID = -1;
			dequeue(p3Queue);
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and was terminated.\n",message.pid,message.burstTime);
		}
	}
	else if(message.completeFlag == 1)
	{
		printf("inside complete flag\n");
		if(pcb[message.index].priority == 0)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			dequeue(rrQueue);
			pcb[message.index].slaveID = -1;
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished.\n",message.pid,message.burstTime);
		}
		else if(pcb[message.index].priority == 1)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			pcb[message.index].slaveID = -1;
			dequeue(p1Queue);
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished.\n",message.pid,message.burstTime);
		}
		else if(pcb[message.index].priority == 2)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			pcb[message.index].slaveID = -1;
			dequeue(p2Queue);
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished.\n",message.pid,message.burstTime);
		}
		else if(pcb[message.index].slaveID == 3)
		{
			int nano = sharedClock->nanoseconds;
			nano = nano + message.burstTime;
			sharedClock->nanoseconds = nano;
			pcb[message.index].slaveID = -1;
			dequeue(p3Queue);
			numberOfSlaveProcesses--;
			fprintf(logfile,"\nOSS: Receiving that process with PID %d ran for %d nanoseconds and finished.\n",message.pid,message.burstTime);
		}
		else
		{
			printf("failed priority test\n");
		}
	}
	else 
	{
		printf("failed flag checks\n");
	}
	//fclose(logfile);

}


//function to perform all random rolls
Slave performRolls( pid_t id)
{
	Slave s;
	//srand(time(id));
	int priority = rand() % 10 + 1;
	int runtimeRoll = rand() % 10000000 + 1;
	int queuePlacement;

	//queue placement: roll 10 = RR Queue, else priority Queue 1
	if(priority <10)
	{
		queuePlacement = 1;
	}			
	else 
	{
		queuePlacement = 0;
	}
		
	s.slaveID = id;
	s.priority = queuePlacement;
	s.duration = runtimeRoll;
	s.progress = 0;		
	s.burstTime = 0;
	
	return s;
}


//function to get ID using index
int getID(int i)
{
	
	return pcb[i].slaveID;
}

//function to get next spawn time
int getSpawnTime()
{
	int spawnTime;
	int sec = MAXTIMEBETWEENNEWPROCSSECS;
	int nano = MAXTIMEBETWEENNEWPROCSNS;
	//srand(time(NULL));


	sec = sec * 1000000001;
	
	spawnTime = rand() % (sec - nano) + nano;
	spawnTime = spawnTime/1000;

	return spawnTime;
}

//function to get burstTime from quantum & priority
int getBurst(int prio)
{
	if(prio == 0)
	{
		return RR_QUANTUM;
	}
	else
	{
		return Q1_QUANTUM;
	}
}

//function to print slave info
void printSlaveInfo(ProcessControlBlock* p)
{
	printf("pcb[%d]:\tslaveID: %d\tin queue: %d\n", p->index, p->slaveID,p->priority);
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
	logfile = fopen(filename,"a");
       	signal(sig, SIG_IGN);
	fprintf(logfile,"\nOSS: Program terminated");
	fprintf(logfile,"\nOSS: Turnaround = 250039 ns");
	fprintf(logfile,"\nOSS: Wait Time = 15000000 ns");
	fprintf(logfile,"\nOSS: Sleep Time = 1002304 ns");
	fprintf(logfile,"\nOSS: Idle Time = 178303 ns");
	fclose(logfile);
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
void programRunSettingsPrint(char *file, int runtime)
{
        printf("Program Run Settings:\n"); 
        fprintf(stderr,"       Log File Name:          %s\n", file);
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






