#include <stdlib.h>
#include <errno.h>

#include "plat.h"
#ifdef __unix__
#include <pthread.h>
#ifdef __STDC_NO_ATOMICS__
#error "<stdatomic.h> required for compilation"
#endif
#include <stdatomic.h>
#endif

#include "sync.h"
#include "error.h"

#define SITH_LOCK_POISON 0xDEAD
#define SITH_LOCK_NORMAL 0

//------------------------------------------------------------------------------
// SEMAPHORES

struct sith_sem {
#ifdef _WIN32
    HANDLE impl;
#elif defined __unix__
    pthread_mutex_t lock;
    pthread_cond_t cv;
    unsigned int value;
    unsigned int max;
    _Atomic unsigned int poison_flag;
#endif
};

SemObject* CreateSemObject(int init, int max) {
    if (init > max) {
        errno = EINVAL;
        return NULL;
    }

    SemObject* this = malloc(sizeof (SemObject));
    if (this == NULL) return NULL;
#ifdef _WIN32
    this->impl = CreateSemaphore(NULL, init, max, NULL);
    if (this->impl == NULL) {
        free(this);
        return NULL;
    }

#elif defined __unix__
    if (init > max) {
        free(this);
        return NULL;
    }
    
    atomic_init(&(this->poison_flag), SITH_LOCK_NORMAL);
    
    int error = pthread_mutex_init(&(this->lock), NULL);
    if (error) {
        free(this);
        errno = error;
        return NULL;
    }
    error = pthread_cond_init(&(this->cv), NULL);
    if (error) {
        pthread_mutex_destroy(&(this->lock));
        free(this);
        errno = error;
        return NULL;
    }

    this->max = max;
    this->value = init;
#endif
    return this;
}

int WaitSemObject(SemObject* this, int blocking) {
#ifdef _WIN32
    DWORD res = WaitForSingleObject(this->impl, blocking ? INFINITE : 0);

    if (res == WAIT_OBJECT_0) return SITH_RET_OK;
    else if (res == WAIT_TIMEOUT && !blocking) return 1;
    else return SITH_RET_ERR;
#elif defined __unix__
    if (atomic_load(&(this->poison_flag)) == SITH_LOCK_POISON) {
        errno = ECANCELED;
        return SITH_RET_ERR;
    }
    int error = pthread_mutex_lock(&(this->lock));
    if (error) {
        errno = error;
        return SITH_RET_ERR;
    }
    while (this->value == 0) {
        if (blocking) {
            error = pthread_cond_wait(&(this->cv), &(this->lock));
            if (error) {
                errno = error;
                return SITH_RET_ERR;
            }
        }
        else {
            pthread_mutex_unlock(&(this->lock));
            errno = EAGAIN;
            return SITH_RET_ERR;
        }
    }

    (this->value)--;
    error = pthread_mutex_unlock(&(this->lock));
    if (error) {
        errno = error;
        return SITH_RET_ERR;
    }

    return SITH_RET_OK;
#endif
}

int SignalSemObject(SemObject* this) {
#ifdef _WIN32
    BOOL res = ReleaseSemaphore(this->impl, 1, NULL);

    // Do check if semaphore actually reached its max
    return res == TRUE ? SITH_RET_OK : (GetLastError() == NOERROR ? 1 : SITH_RET_ERR);

#elif defined __unix__
    if (atomic_load(&(this->poison_flag)) == SITH_LOCK_POISON) {
        errno = ECANCELED;
        return SITH_RET_ERR;
    }
    int error = pthread_mutex_lock(&(this->lock));
    if (error) {
        errno = error;
        return SITH_RET_ERR;
    }

    if (this->value == this->max) {
        pthread_mutex_unlock(&(this->lock));
        return 1;
    }

    (this->value)++;

    error = pthread_mutex_unlock(&(this->lock));
    if (error) {
        errno = error;
        return SITH_RET_ERR;
    }

    pthread_cond_broadcast(&(this->cv));
    return SITH_RET_OK;
#endif
}

int DestroySemObject(SemObject* this) {
#ifdef _WIN32
    BOOL success = CloseHandle(this->impl);
    free(this);
    
    return success ? SITH_RET_OK : SITH_RET_ERR;
    
#elif defined __unix__
    if (atomic_load(&(this->poison_flag)) == SITH_LOCK_POISON) {
        errno = ECANCELED;
        return SITH_RET_ERR;
    }
    
    int error = pthread_mutex_lock(&(this->lock));
    if (error) {
        errno = error;
        return SITH_RET_ERR;
    }
    
    // TDNX
    atomic_exchange(&(this->poison_flag), SITH_LOCK_POISON);
    error = pthread_cond_destroy(&(this->cv));
    int er = pthread_mutex_unlock(&(this->lock));
    int er1 = pthread_mutex_destroy(&(this->lock));
    free(this);
    
    if (error) {
        errno = error;
        return SITH_RET_ERR;
    }
    else if (er) {
        errno = er;
        return SITH_RET_ERR;
    }
    else {
        errno = er1;
        return er1 ? SITH_RET_ERR : SITH_RET_OK;
    }
    
#endif
}


//------------------------------------------------------------------------------
// LOCKS (MUTEXES)

struct sith_mutex {
#ifdef _WIN32
    CRITICAL_SECTION impl;
#elif defined __unix__
    pthread_mutex_t impl;
#endif
};

LockObject* CreateLockObject() {
    LockObject* obj = malloc(sizeof (LockObject));
    if (obj == NULL) return NULL;
#ifdef _WIN32
    InitializeCriticalSection(&(obj->impl));
#elif defined __unix__
    pthread_mutex_init(&(obj->impl), NULL);
#endif
    return obj;
}

void DoLockObject(LockObject* obj) {
#ifdef _WIN32
    EnterCriticalSection(&(obj->impl));
#elif defined __unix__
    pthread_mutex_lock(&(obj->impl));
#endif
}

void DoUnlockObject(LockObject* obj) {
#ifdef _WIN32
    LeaveCriticalSection(&(obj->impl));
#elif defined __unix__
    pthread_mutex_unlock(&(obj->impl));
#endif
}

void DestroyLockObject(LockObject* obj) {
#ifdef _WIN32
    DeleteCriticalSection(&(obj->impl));
#elif defined __unix__
    pthread_mutex_destroy(&(obj->impl));
#endif
    free(obj);
}


//------------------------------------------------------------------------------
// CONDITION VARIABLES

typedef struct sith_cv {
#ifdef _WIN32
    CONDITION_VARIABLE impl;
#elif defined __unix__
    pthread_cond_t impl;
#endif
} CondVar;

CondVar* CreateConditionVar() {
    CondVar* this = malloc(sizeof (CondVar));
    if (this == NULL) return NULL;

#ifdef _WIN32
    InitializeConditionVariable(&(this->impl));
#elif defined __unix__
    int error = pthread_cond_init(&(this->impl), NULL);
    if (error) {
        free(this);
        return NULL;
    }
#endif
    return this;
}

int WaitConditionVariable(CondVar* this, LockObject* lock) {
#ifdef _WIN32
    BOOL success = SleepConditionVariableCS(&(this->impl), &(lock->impl), INFINITE);
    if (!success) {
        return SITH_RET_ERR;
    }
#elif defined __unix__
    int error = pthread_cond_wait(&(this->impl), &(lock->impl));
    if (error) {
        return SITH_RET_ERR;
    }
#endif
    return SITH_RET_OK;
}

int NotifyConditionVariable(CondVar* this) {
#ifdef _WIN32
    WakeConditionVariable(&(this->impl));
#elif defined __unix__
    int error = pthread_cond_signal(&(this->impl));
    if (error) {
        return SITH_RET_ERR;
    }
#endif
    return SITH_RET_OK;
}

int DestroyConditionVar(CondVar* this) {

#ifdef _WIN32
    // Nothing to do
#elif defined __unix__
    int error = pthread_cond_destroy(&(this->impl));
    if (error) {
        return SITH_RET_ERR;
    }
#endif
    free(this);
    return SITH_RET_OK;
}
