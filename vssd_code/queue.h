#ifndef _QUEUE_H
#define _QUEUE_H

typedef struct Queue {
        int capacity;
        int size;
        int front;
        int tail;
        int *elements;
} Queue;


#endif

