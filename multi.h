/* 
 * File:   multi.h
 * Author: Project2100
 * Brief:  Abstraction over primitives for process and thread creation
 * 
 * Implementation notes:
 * 
 *  - [UNIX]: We do not use posix_spawn for process creation, because of
 *      missing specifications with waitpid() related macros
 * 
 * Created on 17 October 2017, 15:16
 */

#ifndef SITH_MULTI_H
#define SITH_MULTI_H

#ifdef __cplusplus
extern "C" {
#endif


//------------------------------------------------------------------------------
// INCLUDES

#include "plat.h"
#include "string.h"

//------------------------------------------------------------------------------
// DEFINITIONS

typedef struct sith_thread ThreadObject;
typedef struct sith_process ProcessObject;


#ifdef _WIN32

// From minwinbase.h for reference:
//typedef DWORD (WINAPI *LPTHREAD_START_ROUTINE)(LPVOID);
typedef LPTHREAD_START_ROUTINE Runnable;
typedef DWORD ThreadValue;
typedef DWORD ProcessValue;
#define SITH_STRFMT_PROCVALUE "lu"
#define SITH_THREAD_CALLCONV //WINAPI

typedef DWORD ProcessID;
#define SITH_STRFMT_PID "lu"
#define SITH_MAXCH_PID SITH_MAXCH_ULONG
#elif defined __unix__


typedef void* (*Runnable)(void*);
typedef void* ThreadValue;
typedef int ProcessValue;
#define SITH_STRFMT_PROCVALUE "i"
#define SITH_THREAD_CALLCONV

typedef pid_t ProcessID;
#define SITH_STRFMT_PID "i"
#define SITH_MAXCH_PID SITH_MAXCH_INT
#endif

//------------------------------------------------------------------------------
// RETURN VALUES


#ifdef _WIN32

#define SITH_RV_ZERO 0
#define SITH_RV_ONE 1
#define SITH_RV_MISSING 2

#elif defined __unix__

#define SITH_RV_ZERO NULL
#define SITH_RV_ONE (void*) 1
#define SITH_RV_MISSING 2

#endif

//------------------------------------------------------------------------------
// FUNCTIONS

/**
 * Creates a new thread inside the process, and starts it immediately.
 * 
 * @param task a pointer to a function to be used as main for the new thread
 * @param argument the argument that will be passed to the function pointed by task at thread start
 * @return The newly created thread object on success, NULL otherwise
 */
ThreadObject* SpawnThread(
        _In_ Runnable task,
        _In_ void* argument);

/**
 * Blocks the calling thread until the given thread terminates
 * 
 * @param thread the thread to be joined
 * @param returnValue the value returned from thread exit, the type is OS-dependant
 * @return the return code of the given thread
 */
int WaitForThread(
        _In_ ThreadObject* thread,
        _Out_opt_ ThreadValue* returnValue);


/**
 * Gets the calling process' ID
 * 
 * @return 
 */
ProcessID GetSelfPID();

/**
 * Spawns a new process
 * 
 * @param argc
 * @param argv
 * @return 
 */
ProcessObject* CreateProcObject(int argc, char* const* argv);

/**
 * Waits for the given process to terminate
 * 
 * @param process
 * @param value
 * @return 0 if successful, -1 otherwise
 */
int WaitForProcObject(ProcessObject* process, ProcessValue* value);

/**
 * Send a signal to the specified thread.</br>
 * Remarks: Has no effect on Windows
 * 
 * @param the thread to send the signal
 * @param signal, the code of the signal to send
 */
void SignalThread(ThreadObject* thread, int signal);

/**
 * Exits the calling thread
 * 
 * - Note: On UNIX, this function enables seamless disambiguation between
 *      a plain return and a pthread_exit() invocation
 * 
 * @param code
 * @return 
 */
int ReturnThread(ThreadValue code);

#ifdef __cplusplus
}
#endif

#endif /* SITH_MULTI_H */
