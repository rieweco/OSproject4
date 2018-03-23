//node.h
//contains structures for children and clock being used in shared memory
#ifndef NODE_H
#define NODE_H

//shared memory keys
#define PCB_KEY 1200
#define CLOCK_KEY 1400


//child struct that holds the child number and its pid
typedef struct ProcessControlBlock
{
        int childNumber;
        pid_t childID;
	int priority;
	bool isBlocked;
}
ProcessControlBlock;


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
