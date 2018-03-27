//slave.c file
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "node.h"
#include <sys/shm.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

//runtime vars
int burstTime;
int duration;
int progress;
int priority;

int queue;
ProcessControlBlock *pcb;
int pcbMemoryID;
int processID;
int setPriority(int p);
void int_Handler(int sig);
int completeCheck(int p, int d);
Message recMSG(int type);
void sendMSG(int type,int found,int rTime, int bTime,int cFlag,int bFlag,int mFlag,int tFlag,int dur,int pr,int id);	

int main(int argc, char *argv[])
{	
	//make sure arg = 2... argv[1] is the process id
	if(argc < 2)
	{
		printf("arg[0]: %s, arg[1]: %s\n",argv[0], argv[1]);
		printf("inside Slave: incorrect number of args!\n");
		exit(1);	
	}
	
	printf("inside Slave\n");
	//processID = argv[0];
	processID = atoi(argv[1]);
	printf("processID: %d\n",processID);
	
	//set up msg queue
	queue = msgget(QUEUEKEY,0666);

	//set up shared memory segment for pcb
	pcbMemoryID = shmget(PCB_KEY, 2048, 0666);
	if(pcbMemoryID < 0)
	{
		perror("inside Slave: Creating PCB shared memory Failed!!\n");
		exit(1);
	}
	pcb = (ProcessControlBlock *)shmat(pcbMemoryID,NULL,0);
		
	
	
	//loop to run critical section 
	fprintf(stderr,"SlaveID: %d - Slave process running!\n", processID); 

	
	while(1)
	{
		Message msg;
		msg = recMSG(processID);
		int foundIndex = msg.index;
		int quantum;
		burstTime = msg.burstTime;
		//priority = msg.priority;
		duration = msg.duration;
		int resume = 0;
		int completeFlag = 0;
		int blockFlag = 0;
		int moveFlag = 0;
		int terminateFlag = 0;
		printf("inside slave: index: %d, burstTIme: %d, duration: %d\n",foundIndex,burstTime,duration);
		/*
		//get quantum value from pcb
		for(i = 0; i < 18; i++)
		{
			printf("inside slave: pcb[%d].slaveID = %d\n",i,pcb[i].slaveID);
			if(pcb[i].slaveID == processID)
			{
				foundIndex = i;
				burstTime = pcb[i].burstTime;
				progress = pcb[i].progress;
				duration = pcb[i].duration;
				priority = pcb[i].priority;
				printf("processID %d found at index %d\n",processID, i);
			}
		}
		*/
		//terminate?
		int terminate = (rand() % 1000) + 1;
		if(terminate > 990)
		{
			//yes - roll how much timeslice
			quantum = burstTime;
			burstTime = (rand() % quantum) + 1;
			progress = progress + burstTime;
			//pcb[i].progress = progress;
			terminateFlag = 1;
		}
		else
		{
			//no - roll for blocked
			//blocked roll
			int blocked = (rand() % 100) + 1;
			quantum = burstTime;
			if(blocked > 90)
			{
				//yes - roll how much time was used
				burstTime = ((rand() % 100) + 1) * quantum;
				burstTime = burstTime/100;
				progress = progress + burstTime;
				//roll resume time
				resume = (rand() % 2000000000) + 1;
				//pcb[i].progress = progress;
				//pcb[i].isBlocked = 1;
				blockFlag = 1;
			}
			else
			{
				//no - compare progress to duration
				int preBurst = progress;
				burstTime = quantum;
				progress = progress + burstTime;
				//pcb[i].progress = progress;
				//if progress is  less than duration, set move flag
				int complete = completeCheck(progress,duration);
				if(complete == 0)
				{
					moveFlag = 1;
					//pcb[i].priority = setPriority(priority);
				}
				else
				{
					burstTime = duration - preBurst;
					printf("process complete, burstTime changed to %d\n",burstTime);
					//progress >= duration, set done flag
					completeFlag = 1;
				}
				
			}
		}
	
		//send message to master
		sendMSG(MASTERKEY,foundIndex,resume,burstTime,completeFlag,blockFlag,moveFlag,terminateFlag,duration,progress,processID);	
	}
	return 0;

}

//function for int handler
void int_Handler(int sig)
{
	exit(0);
}


//function to set new priority if quantum was used
int setPriority(int p)
{	
	int newPriority = p;
	if(newPriority == 0)
	{
		return 0;
	}
	else if(newPriority < 3)
	{
		return newPriority+1;
	}
	else
	{
		return newPriority;
	}
}

//function to check if process has completed it runtime
int completeCheck(int p, int d)
{
	int dura = d;
	int prog = p;
	if(prog >= dura)
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

//function to recieve message from master
Message recMSG(int type)
{
	Message message;
	int msgsize = sizeof(Message);
	msgrcv(queue,&message,msgsize,type,0);
		
	printf("received message from master!\n");
	printf("message index = %d, duration = %d, burstTime = %d\n",message.index,message.duration,message.burstTime);
	
	return message;
}

//function for sending message to master

void sendMSG(int type,int found,int rTime,int bTime,int cFlag,int bFlag,int mFlag,int tFlag,int dur,int pr,int id)
{
	Message message;
	int msgsize = sizeof(Message);
	message.type = type;
	message.pid = id;
	message.index = found;
	printf("inside slave msg: index = %d\n",message.index);
	message.resumeTime = rTime;
	message.burstTime = bTime;
	message.completeFlag = cFlag;
	message.blockFlag = bFlag;
	message.moveFlag = mFlag;
	message.terminateFlag = tFlag;
	message.duration = dur;
	message.progress = pr;

	msgsnd(queue,&message,msgsize,0);
 
}





















