#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <stdatomic.h> 
#include "wbq.h"

// Do your WorkBalancerQueue implementation here. 
// Implement the 3 given methods here. You can add 
// more methods as you see necessary.

void submitTask(WorkBalancerQueue* q, Task* _task) {
	// TODO: Implement submitTask
    // .
    // .
    QueueNode* temp = malloc(sizeof(QueueNode));
    temp->duty = _task;
    temp->next = NULL;

    pthread_mutex_lock(&(q->mutex_tail));
    q->bottom->next = temp;
    q->bottom = temp;
    
    atomic_fetch_add(&(q->size), 1);
    
    pthread_mutex_unlock(&(q->mutex_tail));
}

Task* fetchTask(WorkBalancerQueue* q) {
	// TODO: Implement fetchTask
    // .
    // .
    pthread_mutex_lock (&(q->mutex_head));
    QueueNode* temp = q->top;
    QueueNode* new_head = temp->next;
    if (new_head == NULL)
    {
        pthread_mutex_unlock(&(q->mutex_head));
        return NULL;
    }

    q->top = new_head;
    
    atomic_fetch_sub(&(q->size), 1);

    Task* task = q->top->duty;
    free(temp);
    pthread_mutex_unlock(&(q->mutex_head));

    return task;
}

Task* fetchTaskFromOthers(WorkBalancerQueue* q) {
	// TODO: Implement fetchTaskFromOthers
    // .
    // .
    pthread_mutex_lock(&(q->mutex_tail));
    pthread_mutex_lock(&(q->mutex_head));
    
    // If the queue is empty or only one element is left (top == bottom), we return NULL
    if (q->top == q->bottom) {
        pthread_mutex_unlock(&(q->mutex_tail));
        pthread_mutex_unlock(&(q->mutex_head));
        return NULL;
    }

    QueueNode* temp = q->top;
    while (temp->next != q->bottom) {
        temp = temp->next;
    }

    Task* task = q->bottom->duty;

    free(q->bottom);
    q->bottom = temp;
    q->bottom->next = NULL;

    atomic_fetch_sub(&(q->size), 1);

    pthread_mutex_unlock(&(q->mutex_tail));
    pthread_mutex_unlock(&(q->mutex_head));

    return task;
}