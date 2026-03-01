#pragma once
#define MAX_SIZE 300
#include "procs.h"

typedef struct {
    int items[MAX_SIZE];
    int front;
    int rear;
} Queue;

inline HANDLE initializeQueue(Queue** q, _proc *procList)
{
    HANDLE heap = GetProcessHeapCustomWrapper(procList);

    *q = (Queue*)HeapAllocCustomWrapper(heap, HEAP_ZERO_MEMORY, sizeof(Queue), procList);
    
    (**q).front = -1;
    (**q).rear = 0;

    return CreateMutexWWrapper(NULL, FALSE, NULL, procList);
}

inline int isEmpty(Queue* q) { return (q->front == q->rear - 1); }

inline int isFull(Queue* q) { return (q->rear == MAX_SIZE); }

inline int enqueue(Queue* q, int value, HANDLE queueMutex, _proc* procList)
{
    WaitForSingleObjectWrapper(queueMutex, INFINITE, procList);
    if (isFull(q)) {
        
        ReleaseMutexWrapper(queueMutex, procList);
        return -1;
    }
    q->items[q->rear] = value;
    q->rear++;
    
    ReleaseMutexWrapper(queueMutex, procList);
    return 0;
}

inline int dequeue(Queue* q, HANDLE queueMutex, _proc* procList)
{ 
    WaitForSingleObjectWrapper(queueMutex, INFINITE, procList);
    if (isEmpty(q)) {
        ReleaseMutexWrapper(queueMutex, procList);
        return -1;
    }
    int result = q->items[q->front + 1];
    q->front++;
    ReleaseMutexWrapper(queueMutex, procList);
    return result;
}

//// Function to print the current queue
//void printQueue(Queue* q);