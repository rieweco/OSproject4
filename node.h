//node.h
//contains structures for children and clock being used in shared memory
#ifndef NODE_H
#define NODE_H

//shared memory keys
#define PCB_KEY 1000
#define CLOCK_KEY 1400
#define MASTERKEY 123
#define QUEUEKEY 1227

//pcb struct that holds the slave pid, number, priority, and isblocked
typedef struct ProcessControlBlock
{
        int index;
        pid_t slaveID;
	int priority;
	int isBlocked;
	int burstTime;
	int resumeTime;
	int duration;
	int progress;
}
ProcessControlBlock;

//slave struct that holds all randomly rolled results
typedef struct Slave
{
	pid_t slaveID;
	int priority;
	int duration;
	int progress;
	int burstTime;
	int index;
}
Slave;

//clock struct to track seconds and nanoseconds
typedef struct Clock
{
        int seconds;
        int nanoseconds;
}
Clock;

//message struct that contains the message with values changed/set
typedef struct Message
{
        long type;
	int pid;
        int index;
        int resumeTime;
	int burstTime;
        int completeFlag;
	int blockFlag;
	int moveFlag;
	int terminateFlag;
	int priority;
	int duration;
	int progress;
}
Message;

typedef struct Queue
{
        int front;
        int rear;
        int size;
        unsigned capacity;
        int* array;
}Queue;



#endif
