#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "error.h"

#ifdef __unix__
#include <pthread.h>
#include <sys/wait.h>
#endif

#include "multi.h"


//------------------------------------------------------------------------------
// PROCESSES

struct sith_process {
#ifdef _WIN32
    PROCESS_INFORMATION procInfo;
    STARTUPINFO procSU;
#elif defined __unix__
    pid_t descriptor;
#endif
};


// IMPLNOTES: The spawned process' environment is inherited
// arguments must be "NULL" terminated
// arguments must start with the process' image path

ProcessObject* CreateProcObject(int argc, char*const* argv) {

    // Do check if argv is NULL-terminated
    // argv MUST contain at least one string: the executable's pathname
    if (argv == NULL || argv[0] == NULL || argv[argc] != NULL) {
        errno = EINVAL;
        return NULL;
    }
    // Not checking lengths, the system calls will do all the work for us

    ProcessObject* this = malloc(sizeof (ProcessObject));
    if (this == NULL) return NULL;

#ifdef _WIN32

    // Flatten arguments with the application path, as if writing
    // the whole command in a terminal
    char* comm = flatten(argv, argc, ' ');
    memset(&(this->procInfo), 0, sizeof (PROCESS_INFORMATION));
    memset(&(this->procSU), 0, sizeof (STARTUPINFO));
    this->procSU.cb = sizeof (STARTUPINFO);

    // Passing NULL to environment argument cases the child process to inherit
    // the parent's environment
    BOOL success = CreateProcess(argv[0], comm, NULL, NULL, FALSE, NORMAL_PRIORITY_CLASS, NULL, NULL, &(this->procSU), &(this->procInfo));

    free(comm);

    if (!success) {
        free(this);
        return NULL;
    }

#elif defined __unix__

    int id = fork();
    if (id == 0) {
        int res = execv(argv[0], argv);
        if (res) {
            HandleErrorStatus("exec failed");
            exit(EXIT_FAILURE);
        }
    }
    if (id == -1) {
        free(this);
        return NULL;
    }
    this->descriptor = id;
#endif

    return this;
}

int WaitForProcObject(ProcessObject* this, ProcessValue* value) {
    if (this == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

#ifdef _WIN32
    DWORD result = WaitForSingleObject(this->procInfo.hProcess, INFINITE);

    if (result == WAIT_FAILED) {
        return SITH_RET_ERR;
    }

    // Process has actually terminated
    if (value != NULL)
        // Retrieve the process' exit value
        GetExitCodeProcess(this->procInfo.hProcess, value);

    // Close all handles
    CloseHandle(this->procInfo.hThread);
    CloseHandle(this->procInfo.hProcess);
    CloseHandle(this->procSU.hStdError);
    CloseHandle(this->procSU.hStdInput);
    CloseHandle(this->procSU.hStdOutput);
    free(this);
    return SITH_RET_OK;

#elif defined __unix__
    
#if (_POSIX_VERSION >= 200809L) 
#pragma message "Using waitid implementation"

    siginfo_t status;
    int result = waitid(P_PID, this->descriptor, &status, WEXITED);
    printf("%d\n", result);
    if (result == -1)
        // waitid invocation has failed, leave the child alone
        return SITH_RET_ERR;
    else {
        free(this);
        // NOTE: WINAPI implementation is rather flat, so we're duplicating behaviour here, for now
        switch (status.si_code) {
            case CLD_EXITED:
                // Child has returned by itself, return exit code
                if (value != NULL) *value = status.si_status;
                break;
            case CLD_KILLED:
            case CLD_DUMPED:
                // Child has been killed, return signal code
                if (value != NULL) *value = status.si_status;
                return SITH_RET_ERR;
            default:
                // CLD_CONTINUED and CLD_STOPPED are not checked, invocation only asked for terminated processes
                // CLD_TRAPPED is not contemplated, we're not debugging anything :)
                //fprintf(stderr, "INTERNAL ERROR: Unexpected value in process exit code type: %d", status.si_code);
                *value = 0;
                errno = ENOTRECOVERABLE;
                return SITH_RET_ERR;
        }
        return SITH_RET_OK;
    }

#else
#pragma message "Using waitpid implementation"

    int status = 0;
    int result = waitpid(this->descriptor, &status, 0);

    if (result == -1)
        // waitid invocation has failed, leave the child alone
        return SITH_RET_ERR;

    free(this);

    if (WIFEXITED(status)) {
        if (value != NULL) *value = WEXITSTATUS(status);
        return SITH_RET_OK;
    }
    else if (WIFSIGNALED(status)) {
        if (value != NULL) *value = WTERMSIG(status);
        return SITH_RET_ERR;
    }
    else {
        // Stopped and continued processes are not considered here, we didn't ask for them
        //printf("Unexpected status (0x%x)\n", status);
        errno = ENOTRECOVERABLE;
        return SITH_RET_ERR;
    }

#endif
    
    
#endif
}

ProcessID GetPID(ProcessObject* this) {
    if (this == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }
#ifdef _WIN32
    return this->procInfo.dwProcessId;
#elif defined __unix__
    return this->descriptor;
#endif

}

ProcessID GetSelfPID() {
#ifdef _WIN32
    return GetCurrentProcessId();
#elif defined __unix__
    return getpid();
#endif
}


//------------------------------------------------------------------------------
// THREADS

struct sith_thread {
#ifdef _WIN32
    HANDLE handle;
    DWORD descriptor;
#elif defined __unix__
    pthread_t descriptor;
#endif
};

ThreadObject* SpawnThread(Runnable threadMain, void* argument) {
    if (threadMain == NULL) {
        errno = EINVAL;
        return NULL;
    }

    ThreadObject* obj = malloc(sizeof (ThreadObject));
    if (obj == NULL) return NULL;
#ifdef _WIN32
    obj->handle = CreateThread(NULL, 0, threadMain, argument, 0, &(obj->descriptor));
    if (obj->handle == NULL) {
        free(obj);
        return NULL;
    }
#elif defined __unix__
    errno = pthread_create(&(obj->descriptor), NULL, threadMain, argument);
    // AP170811: WARNING - Shortcutting error code to errno
    if (errno) {
        free(obj);
        return NULL;
    }
#endif
    return obj;
}

int WaitForThread(ThreadObject* thread, ThreadValue* out) {

#ifdef _WIN32
    if (WaitForSingleObject(thread->handle, INFINITE) == WAIT_FAILED) {
        // NOTE: Abandoned and timeout codes are ignored, they shouldn't come up by spec
        // We assume the thread is still running
        return SITH_RET_ERR;
    };

    //Thread has terminated, get exit code
    BOOL success;
    if (out != NULL) {
        if (GetExitCodeThread(thread->handle, out) == 0) *out = SITH_RV_MISSING;
    }

    success = CloseHandle(thread->handle);
    free(thread);
    return success ? SITH_RET_OK : SITH_RET_ERR;

#elif defined __unix__
    // AP170811: WARNING - Shortcutting to errno, see previous function
    errno = pthread_join(thread->descriptor, out);
    free(thread);
    return errno ? SITH_RET_ERR : SITH_RET_OK;

#endif
}

void SignalThread(ThreadObject* thread, int signal) {
#ifdef _WIN32
    (void) thread;
    (void) signal;
#elif defined __unix__
    if (thread != NULL) pthread_kill(thread->descriptor, signal);
#endif

}

int ReturnThread(ThreadValue code) {
#ifdef _WIN32
    ExitThread(code);
#elif defined __unix__
    pthread_exit(code);
#endif
}
