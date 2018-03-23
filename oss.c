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

//function declarations
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
        int sharedMemoryID;
	ProcessControlBlock *pcb;	
	Clock *clock;
        int total = 0;
        int prCount = 0;
        int opt = 0;
        int numberOfSlaveProcesses = DEFAULT_SLAVE;
        char *filename = DEFAULT_FILENAME;
        int runtime = DEFAULT_RUNTIME;
        pid_t master = 1;

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
	
	//set up shared memory segment for clock
	sharedMemoryID = shmget(MEMORY_KEY, sizeof(Clock), IPC_CREAT | 1000);
	if(sharedMemoryID < 0)
	{
		perror("Creatinf shared memory fragment Failed!!\n");
		exit(errno);
	}

        int count = numberOfSlaveProcesses;
        int i = 0;
        
	

	

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

        return 0;
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
	printf("Alarm! Time is UP!\n");
	for(int i = 0; i < numSlaveProcesses; i++)
	{
		kill(pcd[i].childID, SIGINT);	
	}
	
	while(wait(
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






