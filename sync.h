/* 
 * File:   sync.h
 * Author: Project2100
 * Brief:  Interface to all data structures and methods concerning
 *           threads, semaphores and mutexes
 * 
 * 
 * Implementation notes:
 * 
 * - [UNIX]: Using POSIX semaphores
 * 
 * - [UNIX]: On mutexes, default attributes are kept for performance:
 * http://pubs.opengroup.org/onlinepubs/9699919799/functions/pthread_mutexattr_destroy.html#
 * 
 *
 * Created on 11 August 2017, 15:18
 */

#ifndef SITH_SYNC_H
#define SITH_SYNC_H

#ifdef __cplusplus
extern "C" {
#endif


//------------------------------------------------------------------------------
// INCLUDES & DEFINITIONS


typedef struct sith_sem SemObject;
typedef struct sith_mutex LockObject;
typedef struct sith_cv CondVar;


//------------------------------------------------------------------------------
// FUNCTIONS

/**
 * Creates a Monitor object
 * 
 * @param init
 * @param max
 * @return a new Monitor object, or NULL if creation failed
 */
SemObject* CreateSemObject(
        _In_ int init,
        _In_ int max);

/**
 * Blocks the calling thread and waits for the given Monitor to be signalled
 * 
 * @param mon
 * @return 0 if successful, -1 otherwise
 */
int WaitSemObject(
        _In_ SemObject* mon, 
        _In_ int blocking);

/**
 * Signals the given Monitor
 * 
 * @param mon
 * @return 0 if successful, -1 otherwise
 */
int SignalSemObject(
        _In_ SemObject* mon);

/**
 * Closes this monitor and releases all resources allocated to it
 * 
 * @param mon
 * @return 0 if successful, -1 otherwise
 */
int DestroySemObject(
        _In_ SemObject* mon);

LockObject* CreateLockObject();
void DoLockObject(LockObject* cs);
void DoUnlockObject(LockObject* cs);
void DestroyLockObject(LockObject* cs);

CondVar* CreateConditionVar();
int WaitConditionVariable(CondVar* this, LockObject* lock);
int NotifyConditionVariable(CondVar* this);
int DestroyConditionVar(CondVar* this);

#ifdef __cplusplus
}
#endif

#endif /* SITH_SYNC_H */
