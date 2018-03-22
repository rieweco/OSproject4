//queue.c
//
// this was code taken from https://www.geeksforgeeks.org/queue-set-1introduction-and-array-implementation/
// I have changed a few things for me to use for this project, namely
// removed a function and renamed a few.

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

//queue struct
struct Queue
{
	int front;
	int rear;
	int size;
	unsigned capacity;
	int* array;
};

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

//driver program to test functions
int main()
{
	struct Queue* queue = generateQueue(1000);

	enqueue(queue, 10);
	enqueue(queue, 20);
	enqueue(queue, 30);
	enqueue(queue, 40);

	printf("%d dequeued from queue\n", dequeue(queue));
	
	printf("front item is %d\n", front(queue));

	return 0;
}











































