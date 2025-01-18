#pragma once

#include <semaphore.h>
#include <iostream>
#include <vector>
#include <unistd.h>

#include <cstdio>
#include <pthread.h>
#include <sstream>

class Tour
{
public:
    Tour(int groupSize, int guided)
    {
        // Check validity of arguments
        if (groupSize <= 0) {
            throw std::invalid_argument("An error occurred.");
        }

        if (guided != 0 && guided != 1) {
            throw std::invalid_argument("An error occurred.");
        }


        this->tour_started = 0;
        this->current_visitors = 0;
        this->group_size = groupSize;
        this->guided = guided;

        
        if (guided)
        {
            sem_init(&sem_arrive, 0, groupSize + 1); 
        }
        else
        {
            sem_init(&sem_arrive, 0, groupSize);
        }
        sem_init(&mutex, 0, 1);
        sem_init(&sem_leave, 0, 0); 
    }

    // Will be provided by you.
    void start();


    void arrive()
    {
        // First take the thread ID and print the information that you have been arrived.
        pthread_t thread_id = pthread_self();
        printf("Thread ID: %lu | Status: Arrived at the location.\n", thread_id);

        // If there is space, enter.
        sem_wait(&sem_arrive);

        // To achieve mutex i used a semaphore with value 1, only one thread can enter the critical code.
        sem_wait(&mutex);
        current_visitors++; // Value increment.

        // If the tour is guided tour
        if (guided)
        {
            if(current_visitors == group_size + 1)  // The tour start when there is gorup_size + 1 people inside when there is a guide.
            {
                guide_ID = thread_id;   // Take the guide id.
                printf("Thread ID: %lu | Status: There are enough visitors, the tour is starting.\n", thread_id);
                tour_started = 1;   // Set the tour as started.
            }
            else
            {
                // If there is not enough people, just travel alone.
                printf("Thread ID: %lu | Status: Only %d visitors inside, starting solo shots.\n", thread_id, current_visitors);
            }
            // Signaling that enother thread can grap the mutex.
            sem_post(&mutex);
        }
        else // If there will be no guide.
        {
            if(current_visitors == group_size) // when there is no guide, we need group_size of people to start the tour.
            {
                guide_ID = thread_id; // Duplicate code, not necessary since we do not use it.
                printf("Thread ID: %lu | Status: There are enough visitors, the tour is starting.\n", thread_id);
                tour_started = 1;
            }
            else
            {
                printf("Thread ID: %lu | Status: Only %d visitors inside, starting solo shots.\n", thread_id, current_visitors);
            }
            sem_post(&mutex);
        }
    }


    void leave()
    {
        sem_wait(&mutex);
        pthread_t thread_id = pthread_self();

        if(!tour_started)
        {
            printf("Thread ID: %lu | Status: My camera ran out of memory while waiting, I am leaving.\n", thread_id);
            current_visitors--;
            // When a visitor finish traveling, it signals so that another one can enter.
            sem_post(&sem_arrive);
            sem_post(&mutex);
        }
        else
        {
            // If we are in tour, we need to ensure only the last visitor signal the others.
            sem_post(&mutex);
            if (guided)
            {
                // In the guided tour, if a visitor is guided
                if(pthread_equal(thread_id, guide_ID))
                {
                    // It should first print the tour is over.
                    printf("Thread ID: %lu | Status: Tour guide speaking, the tour is over.\n", thread_id);
                    current_visitors--;
                    // Then give a signal to other visitors that is in the tour so they also can leave.
                    sem_post(&sem_leave);
                }
                else
                {
                    // If the tour is guided, travelers must wait for the guide.
                    sem_wait(&sem_leave);

                    sem_wait(&mutex);
                    current_visitors--;
                    // When their wait is over, they also signals another visitor. In the end, the value of this semaphore will be one.
                    sem_post(&sem_leave);
                    printf("Thread ID: %lu | Status: I am a visitor and I am leaving.\n", thread_id);
                    if(current_visitors == 0)
                    {
                        // When all visitors leave, the last visitor should close the leave semaphore (make it zero) and set tour to zero.
                        printf("Thread ID: %lu | Status: All visitors have left, the new visitors can come.\n", thread_id);
                        tour_started = 0;
                        sem_wait(&sem_leave);
                        // Also it should signal the arrive semaphore group_size times so that other visitors can enter.
                        for (int i = 0; i < group_size + 1; i++)
                        {
                            sem_post(&sem_arrive);
                        }
                    }
                    sem_post(&mutex);
                }
            }
            else
            {
                sem_wait(&mutex);
                current_visitors--;
                printf("Thread ID: %lu | Status: I am a visitor and I am leaving.\n", thread_id);
                if(current_visitors == 0)
                {
                    printf("Thread ID: %lu | Status: All visitors have left, the new visitors can come.\n", thread_id);
                    tour_started = 0;
                    for (int i = 0; i < group_size; i++)
                    {
                        sem_post(&sem_arrive);
                    }
                }
                sem_post(&mutex);
            }
        }
    }



private:
    int tour_started;
    int current_visitors;
    int group_size;
    int guided;

    pthread_t  guide_ID;
    sem_t mutex;                    // Mutex for synchronization
    sem_t sem_arrive;               // Semaphore for arrival
    sem_t sem_leave;                // Semaphore for leave

};
