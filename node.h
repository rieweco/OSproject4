//node.h
//contains structures for children and clock being used in shared memory
#ifndef NODE_H
#define NODE_H

//shared memory keys
#define PCB_KEY 1000
#define CLOCK_KEY 1400


//pcb struct that holds the slave pid, number, priority, and isblocked
typedef struct ProcessControlBlock
{
        int index;
        pid_t slaveID;
	int priority;
	int isBlocked;
	int start;
	int hasStarted;	
}
ProcessControlBlock;

//slave struct that holds all randomly rolled results
typedef struct Slave
{
	pid_t slaveID;
	int priority;
	int isBlockable;
	int duration;
	int progress;
	int start;
}
Slave;

//clock struct to track seconds and nanoseconds
typedef struct Clock
{
        int seconds;
        int nanoseconds;
}
Clock;

//child message struct that contains the message,
//pid, time of process in ss:nn, and a flag for
//signalling being in/out of the critical section 
typedef struct PassedMessage
{
        long mtype;
        int childNumber;
        int seconds;
        int nanoseconds;
        int flag;
}
PassedMessage;

typedef struct Queue
{
        int front;
        int rear;
        int size;
        unsigned capacity;
        int* array;
}Queue;



#endif
