/* 
 * File:   pool.h
 * Author: Project2100
 * Brief:  Thread-pool implementation based on "multi.h" and "sync.h"
 * 
 * 
 * Implementation notes:
 * 
 *  - Pools created by the same process shall NOT have the same name
 *
 * Created on 22 July 2017, 14:42
 */

#ifndef SITH_POOL_H
#define SITH_POOL_H

#ifdef __cplusplus
extern "C" {
#endif
    

//------------------------------------------------------------------------------
// DEFINITIONS
    
// Markers for pool tasks and task arguments
#define SITH_TASKBODY
#define SITH_TASKARG

typedef struct sith_pool ThreadPool;
// Pool task signature
typedef int (*PoolTask)(void*);


//------------------------------------------------------------------------------
// FUNCTIONS

/**
 * Creates a new thread pool with the given number of threads
 * 
 * Note: On UNIX, all threads inherit the signal mask from the calling thread
 * 
 * @param name
 * @param numThreads
 * @return A pointe to the newly created thread pool, NULL if an error occurred
 */
ThreadPool* CreateThreadPool(char* name, unsigned int numThreads);

/**
 * Submits a task to be executed to this thread pool
 * 
 * @param pool
 * @param task
 * @param argument
 * @param blocking
 * @return 
 */
int ScheduleTask(ThreadPool* pool, PoolTask task, void* argument, int blocking);

/**
 * Changes the number of threads in this pool
 * 
 * @param pool
 * @param newCount
 * @return 
 */
int ResizeThreadPool(ThreadPool* pool, unsigned int newCount);

/**
 * Releases all resources for this thread pool
 * 
 * @param pool
 * @param blocking
 * @return 
 */
int DestroyThreadPool(ThreadPool* pool, int blocking);

/**
 * Returns this thread pool's size
 * 
 * @param this
 * @return 
 */
unsigned int GetThreadPoolSize(ThreadPool* this);

    
#ifdef __cplusplus
}
#endif

#endif /* SITH_POOL_H */
