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

//runtime vars
int start;
int stop;
int duration;
int progress;

Clock *clock;
int clockID;
int processID;
void sendMSG(int, int);
void recMSG(int);
void int_Handler(int sig);



int main(int argc, char *argv[])
{	
	//make sure arg = 2... argv[1] is the process id
	string argument;
	if(argc = 2)
	{
		argv[1] = argument;
		processID = atoi(argument);	
	}
	else
	{
		perror("Child Error: incorrect number of args!\n");
		exit(1);
	}

	//access shared memory for clock
	clockID = shmget(CLOCK_KEY, sizeof(Clock), 0666);
	if(clockID < 0)
	{
		perror("Child Error: shmget error!\n");
		exit(1);	
	}

	//attach to shared memory for clock
	clock = shmat(clockID, NULL, 0);

	//create random number for duration of process
	srand(time(NULL));
	duration = (rand() % 1000000) + 1;
	printf("slaveID: %d Duration set to: %d nanoseconds\n", processID, duration);
	
	progress = 0;
	
	//loop to run critical section 










	return 0;

}






