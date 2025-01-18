#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include "constants.h"
#include "wbq.h"

extern int stop_threads;
extern int finished_jobs[NUM_CORES];
extern int flag_array[NUM_CORES];
extern WorkBalancerQueue** processor_queues;

// Thread function for each core simulator thread
void* processJobs(void* arg) {
    // initalize local variables
    ThreadArguments* my_arg = (ThreadArguments*) arg;
    WorkBalancerQueue* my_queue = my_arg -> q;
    int my_id = my_arg -> id;

    // Main loop, each iteration simulates the processor getting The stop_threads flag
    // is set by the main thread when it concludes all jobs are finished. The bookkeeping 
    // of finished jobs is done in executeJob's example implementation. a task from 
    // its or another processor's queue. After getting a task to execute the thread 
    // should call executeJob to simulate its execution. It is okay if the thread
    //  does busy waiting when its queue is empty and no other job is available
    // outside.
    while (!stop_threads) 
    {
        Task* task = NULL;
        if ((my_queue->size == 0 && my_queue->next->size > 2) 
        ||(my_queue->size + 10 < my_queue->next->size) 
        || (my_queue->size < 3 && (my_queue->size + 2 < my_queue->next->size)) 
        || (my_queue->size < 6 && (my_queue->size + 5 < my_queue->next->size)))
        {
            task = fetchTaskFromOthers(my_queue->next);
        }
        else
        {
            task = fetchTask(my_queue);
            
        }

        int total = finished_jobs[my_id];
        if (task != NULL)
        {
            
            executeJob(task, my_queue, my_id);            
            if (total == finished_jobs[my_id])
            {
                submitTask(my_queue, task);
            }
        }
    }
    
}

// Do any initialization of your shared variables here.
// For example initialization of your queues, any data structures 
// you will use for synchronization etc.
void initSharedVariables() {
    // TODO: Fill in this function according to your needs:
    // .
    // .
    for (int i = 0; i < NUM_CORES; i++)
    {
        //Initialize each queue's pointers and mutex;
        QueueNode * temp = malloc(sizeof(QueueNode));
        temp->next = NULL;
        processor_queues[i]->bottom = temp;
        processor_queues[i]->top = temp;
        
        processor_queues[i]->size = 0;
        pthread_mutex_init(&(processor_queues[i]->mutex_head), NULL);
        pthread_mutex_init(&(processor_queues[i]->mutex_tail), NULL);
        

        if (i != NUM_CORES - 1) processor_queues[i]->next = processor_queues[i+1];
        else processor_queues[i]->next = processor_queues[0];
    }
}