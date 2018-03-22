//node.h
//contains structures for children and clock being used in shared memory
#ifndef NODE_H
#define NODE_H


//child struct that holds the child number and its pid
typedef struct ChildProcess
{
        int childNumber;
        pid_t childID;
}
ChildProcess;


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
