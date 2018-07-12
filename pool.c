#include <stdlib.h>
#include <stdio.h>

#include "error.h"
#include "multi.h"
#include "sync.h"

#include "pool.h"

// Kernel definition

typedef struct sith_node {
    unsigned int id;
    ThreadObject* thread;

    // Useful for list reinsertion
    ThreadPool* owner;

    // Task fields
    PoolTask task;
    void* argument;

    SemObject* semaph;

    // List implementation
    struct sith_node* next;
} Kernel;

// Pool definition

struct sith_pool {
    // Pool name, unused for now
    char* name;

    // Holds pointers to all Kernels of this pool, for easy memmgmt
    Kernel** kernels;

    // This list keeps track of the available threads for scheduling
    Kernel* idleKernels;

    // Using a condition variable to synchronize all threads
    LockObject* lock;
    CondVar* cv;
    
    // Counters for quick checks and reports
    unsigned int idleCount;
    unsigned int kernCount;
};


//------------------------------------------------------------------------------
// POOL HELPERS

// ASSERT: Requires lock

void spin_kernel() {

}

// ASSERT: Requires lock

Kernel* terminate_kernel(Kernel* k) {

    // Force NULL values here, kernel will interpret them as a termination command
    k->task = NULL;
    k->argument = NULL;
    SignalSemObject(k->semaph);

    // Wait for kernel return
    WaitForThread(k->thread, NULL);
    DestroySemObject(k->semaph);
    Kernel* next = k->next;
    free(k);
    return next;
}

// ASSERT: Requires lock
// CAUTION: Releases lock

void deallocate_pool(ThreadPool* pool) {

    LockObject* lock = pool->lock;

    // Free all kernels
    for (Kernel* k = pool->idleKernels; k != NULL; k = terminate_kernel(k)) {
    }
    free(pool->kernels);

    // Deallocate pool struct before exiting the critsection
    DestroyConditionVar(pool->cv);
    free(pool->name);
    free(pool);
    DoUnlockObject(lock);
    DestroyLockObject(lock);
}


//------------------------------------------------------------------------------
// KERNEL BODY

ThreadValue SITH_THREAD_CALLCONV kernelBody(void* n) {

    Kernel* self = (Kernel*) n;
    int error;

    while (1) {

        // Name reporting is prone to race conditions if lock is not held
        /*
                DoLockObject(self->owner->lock);
                printf("[%s_kno%u] Parking\n", self->owner->name, self->id);
                DoUnlockObject(self->owner->lock);
                fflush(stdout);
         */

        // Park thread
        error = WaitSemObject(self->semaph, 1);
        if (error) {
            HandleErrorStatus("Kernel failure on semaphore");
            return SITH_RV_ONE;
        }

        // Thread woken, read task
        // If both are NULL, then the owning pool is terminating
        // Do return normally
        if (self->argument == NULL && self->task == NULL) break;

        // See above
        /*
                DoLockObject(self->owner->lock);
                printf("[%s_kno%u] Executing\n", self->owner->name, self->id);
                DoUnlockObject(self->owner->lock);
                fflush(stdout);
         */

        // Asserting task and argument fields are set
        // invoke task
        int outcome = (*(self->task))(self->argument);

        // Update pool - enter critical
        DoLockObject(self->owner->lock);
        if (outcome) {
            // Report task failure
            fprintf(stderr, "[Kern_%s_%u]: Task failed with code %d\n", self->owner->name, self->id, outcome);
            fflush(stderr);
        }

        self->next = self->owner->idleKernels;
        self->owner->idleKernels = self;
        (self->owner->idleCount)++;
        DoUnlockObject(self->owner->lock);
        NotifyConditionVariable(self->owner->cv);

    }

    return SITH_RV_ZERO;
}


//------------------------------------------------------------------------------
// API FUNCTIONS

ThreadPool* CreateThreadPool(char* name, unsigned int numThreads) {

    // Arg check
    if (name == NULL || numThreads == 0) {
        errno = EINVAL;
        return NULL;
    }

    ThreadPool* pool = malloc(sizeof (ThreadPool));
    if (pool == NULL) {
        return NULL;
    }

    // Give the pool a name, for all its related semaphores
    // Using PID to ensure they are process-private
    size_t namelen = strlen(name) + 1;
    pool->name = malloc(namelen * sizeof (char));
    if (pool->name == NULL) {
        free(pool);
        return NULL;
    }
    memcpy(pool->name, name, namelen);

    pool->cv = CreateConditionVar();
    if (pool->cv == NULL) {
        free(pool->name);
        free(pool);
        return NULL;
    }

    // Allocate space for kernel pointers
    pool->kernels = malloc(numThreads * sizeof (Kernel*));
    if (pool->kernels == NULL) {
        DestroyConditionVar(pool->cv);
        free(pool->name);
        free(pool);
        return NULL;
    }

    // Init pool state
    pool->idleCount = 0;
    pool->kernCount = numThreads;
    pool->lock = CreateLockObject();

    // Kernel construction
    pool->idleKernels = NULL;
    Kernel* kern;
    for (unsigned int kerIndex = 0; kerIndex < numThreads; kerIndex++) {

        kern = calloc(1, sizeof (Kernel));
        if (kern == NULL) {
            deallocate_pool(pool);
            return NULL;
        }

        kern->id = kerIndex;

        kern->semaph = CreateSemObject(0, 1);
        if (kern->semaph == NULL) {
            free(kern);
            deallocate_pool(pool);
            return NULL;
        }

        // All went well (finally), link the kernel to its pool
        kern->owner = pool;

        // Start the kernel
        kern->thread = SpawnThread(kernelBody, kern);
        if (kern->thread == NULL) {
            DestroySemObject(kern->semaph);
            free(kern);
            deallocate_pool(pool);
            return NULL;
        }


        // Register kernel in pool
        pool->kernels[kerIndex] = kern;

        // Chain the idle list to this node - we are doing a prepend action over a list
        kern->next = pool->idleKernels;
        pool->idleKernels = kern;
    }

    //No further actions needed, assume all kernels are ready
    pool->idleCount = numThreads;

    return pool;
}

int ScheduleTask(ThreadPool* pool, PoolTask task, void* argument, int blocking) {
    if (pool == NULL || task == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    int error;

    DoLockObject(pool->lock);

    // Query for available kernels
    while (pool->idleCount == 0) {
        if (!blocking) {
            errno = EAGAIN;
            DoUnlockObject(pool->lock);
            return SITH_RET_ERR;
        }
        error = WaitConditionVariable(pool->cv, pool->lock);
        if (error) {
            return SITH_RET_ERR;
        }
    }

    // If we're here, then we have idle kernels, detach one from the idle list
    Kernel* chosen = pool->idleKernels;
    pool->idleKernels = pool->idleKernels->next;

    // register runnable and argument in the kernel
    chosen->task = task;
    chosen->argument = argument;
    (pool->idleCount)--;
    DoUnlockObject(pool->lock);

    // Detached kernel can start working
    SignalSemObject(chosen->semaph);

    return SITH_RET_OK;
}

int DestroyThreadPool(ThreadPool* pool, int blocking) {
    if (pool == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    DoLockObject(pool->lock);

    // See if there are any active kernels
    while (pool->idleCount != pool->kernCount) {
        // Some threads are still running
        if (!blocking) {
            DoUnlockObject(pool->lock);
            errno = EAGAIN;
            return SITH_RET_ERR;
        }
        WaitConditionVariable(pool->cv, pool->lock);
    }

    // If not, then all kernels are parked, start dismantling the pool
    deallocate_pool(pool);
    return SITH_RET_OK;
}

int ResizeThreadPool(ThreadPool* pool, unsigned int newCount) {
    if (pool == NULL || newCount == 0) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    if (newCount == pool->kernCount) return 0;

    // Critical section entry point
    DoLockObject(pool->lock);

    if (newCount > pool->kernCount) {
        // Extend case, construct new kernels

        // Create a buffer that keeps pointers to he new kernels
        // Guaranteed to be nonzero because of the "if" expression
        Kernel** kernelBuffer = calloc(newCount - pool->kernCount, sizeof (Kernel*));
        if (kernelBuffer == NULL) {
            DoUnlockObject(pool->lock);
            return SITH_RET_ERR;
        }

        // Build the new kernels
        Kernel* kern;
        for (unsigned int kerIndex = 0; kerIndex < newCount - pool->kernCount; kerIndex++) {

            // Allocate space for the object
            kern = calloc(1, sizeof (Kernel));
            if (kern == NULL) {
                goto extendFail;
            }

            kern->id = kerIndex + pool->kernCount;

            // COnstruct its semaphore
            kern->semaph = CreateSemObject(0, 1);
            if (kern->semaph == NULL) {
                free(kern);
                goto extendFail;
            }


            // Spin up the underlying thread
            kern->thread = SpawnThread(kernelBody, kern);
            if (kern->thread == NULL) {
                DestroySemObject(kern->semaph);
                free(kern);
                goto extendFail;
            }

            // Link the kernel to its pool
            kern->owner = pool;

            // Keep track of the kernel in the buffer
            kernelBuffer[kerIndex] = kern;

        }

        // Now, attach the new kernels to the pool

        // Reallocate the kernel tracker
        Kernel** pkAlias = realloc(pool->kernels, newCount * sizeof (Kernel*));
        if (pkAlias == NULL) {
            goto extendFail;
        }
        pool->kernels = pkAlias;

        // We're done with allocations, tie up all loose ends
        for (unsigned int kerIndex = pool->kernCount; kerIndex < newCount; kerIndex++) {

            // Register kernel in pool
            pool->kernels[kerIndex] = kernelBuffer[kerIndex - pool->kernCount];

            // Chain the idle list to this node - we are doing a prepend action over a list
            kernelBuffer[kerIndex - pool->kernCount]->next = pool->idleKernels;
            pool->idleKernels = kernelBuffer[kerIndex - pool->kernCount];

        }
        free(kernelBuffer);

        // Update pool counters
        pool->idleCount += newCount - pool->kernCount;
        pool->kernCount = newCount;
        goto exitPoint;


extendFail:
        // We have failed here, shut down the spawned kernels gracefully
        for (size_t i = 0; i < newCount - pool->kernCount; i++)
            terminate_kernel(kernelBuffer[i]);
        DoUnlockObject(pool->lock);
        return SITH_RET_ERR;
    }

    else {
        //Be careful..
        if (pool->kernCount - newCount > pool->idleCount) {
            DoUnlockObject(pool->lock);
            errno = EAGAIN;
            return SITH_RET_ERR;
        }

        while (newCount != pool->kernCount) {

            // Just terminate an idle kernel
            Kernel* kern = pool->idleKernels;
            unsigned int kid = kern->id;
            pool->idleKernels = pool->idleKernels->next;

            // Update IDs if necessary
            if (kid != pool->kernCount - 1) {
                pool->kernels[pool->kernCount - 1]->id = kid;
                pool->kernels[kid] = pool->kernels[pool->kernCount - 1];
            }
            terminate_kernel(kern);

            pool->idleCount--;
            pool->kernCount--;
        }

        // Now we can directly reallocate the kernel map
        //  - Should be guaranteed to succeed because of shrink-only case and
        //    memory function synchronization
        pool->kernels = realloc(pool->kernels, newCount * sizeof (Kernel*));
    }

exitPoint:
    DoUnlockObject(pool->lock);
    return SITH_RET_OK;
}

unsigned int GetThreadPoolSize(ThreadPool* this) {
    if (this == NULL) {
        return SITH_RET_ERR;
    }
    return this->kernCount;
}
