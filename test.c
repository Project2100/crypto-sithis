/*
 * File:   test.c
 * Author: Project2100
 *
 * Created on 26 Aug 2017, 10:56
 */

#include "error.h"
#include "proto.h"
#include "file.h"
#include "string.h"
#include "string_heap.h"
#include "filewalker.h"
#include "multi.h"
#include "pool.h"
#include "sync.h"
#include "list.h"
#include "arguments.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32

#define SITH_TEST_PROCESS ".\\testaux.exe"
#define SYSPAUSE system("pause");

#elif defined __unix__

#include <signal.h>
#define SITH_TEST_PROCESS "./testaux"
#define SYSPAUSE system("read -n1 -r -p 'Press any key to continue...'");

#endif

#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

int testTask(void* arg) {
    WaitSemObject((SemObject*) arg, 1);
    return 0;
}

// Relies on user input timing

int test_pool() {
    // Creating pool with 2 kernels
    ThreadPool* pool = CreateThreadPool("test", 2);
    if (pool == NULL) {
        HandleErrorStatus("Error creating pool");
        exit(1);
    }

    printf("Pool created\n");

    SYSPAUSE
    // STATUS: 2 Kernels, 2 idle

    // Scheduling a task on each kernel
    SemObject* s1 = CreateSemObject(0, 1);
    if (ScheduleTask(pool, testTask, s1, 1)) {
        HandleErrorStatus("Error on first task");
        exit(1);
    }

    SemObject* s2 = CreateSemObject(0, 1);
    if (ScheduleTask(pool, testTask, s2, 1)) {
        HandleErrorStatus("Error on second task");
        exit(1);
    }

    // STATUS: 2 Kernels, 0 idle

    // Attempting to schedule another task - should fail
    SemObject* s3 = CreateSemObject(0, 1);
    if (ScheduleTask(pool, testTask, s3, 0)) {
        HandleErrorStatus("Third task blocked");
    }
    else {
        printf("OSHI-\n");
        exit(-1);
    }

    // STATUS: 2 Kernels, 0 idle

    // Transmuting to a greater pool
    if (ResizeThreadPool(pool, 4)) {
        HandleErrorStatus("Error transmuting pool");
        exit(1);
    }

    // STATUS: 4 Kernels, 2 idle

    // Scheduling a task on the new kernels
    SemObject* s4 = CreateSemObject(0, 1);
    if (ScheduleTask(pool, testTask, s4, 0)) {
        HandleErrorStatus("Error on fourth task");
        exit(1);
    }

    SemObject* s5 = CreateSemObject(0, 1);
    if (ScheduleTask(pool, testTask, s5, 0)) {
        HandleErrorStatus("Error on fifth task");
        exit(1);
    }

    // STATUS: 4 Kernels, 0 idle
    //BARRIER
    SYSPAUSE

    SignalSemObject(s1);
    SignalSemObject(s2);

    // STATUS: 4 Kernels, 2 idle
    SYSPAUSE


    printf("Reducing\n");
    fflush(stdout);

    // Transmuting to a greater pool
    if (ResizeThreadPool(pool, 2)) {
        HandleErrorStatus("Error transmuting pool");
        exit(1);
    }

    // STATUS: 2 Kernels, 0 idle

    if (ScheduleTask(pool, testTask, s3, 0)) {
        HandleErrorStatus("Third task blocked, again");
    }
    else {
        printf("OSHI-\n");
        exit(-1);
    }

    // STATUS: 2 Kernels, 0 idle
    SYSPAUSE

    SignalSemObject(s3);
    SignalSemObject(s4);
    SignalSemObject(s5);

    DestroyThreadPool(pool, 1);
    DestroySemObject(s1);
    DestroySemObject(s2);
    DestroySemObject(s3);
    DestroySemObject(s4);
    DestroySemObject(s5);

    printf("["COLOR_GREEN"OK"COLOR_RESET"] Thread pool test passed\n");
    return 0;
}

int test_file() {
    File* f = CreateFileObject("\\\\.\\COM1:", SITH_FS_ZERO, SITH_FILEMODE_RO, SITH_OPENMODE_EXIST, 0);
    if (f == NULL) {
        HandleErrorStatus("Could not open COM1");
    }
    return 0;
}

int test_walker() {
    HeapString* string = CreateHeapString("");
    SithWalker* walker = InitWalker("./");
    if (WalkInDirRecursive(walker, string)) {
        printf("["COLOR_RED"FAILED"COLOR_RESET"] Failure recursive directory traversal\n");
        return -1;
    }
    DisposeWalker(walker);
    DisposeHeapString(string);
    printf("["COLOR_GREEN"OK"COLOR_RESET"] FileWalker test passed\n");
    return 0;
}

int test_process() {


    // Process' name
    /*
        char* longname = calloc(65536, sizeof(char));
        memset(longname, ' ', 65535);
        char* args[] = {longname, NULL};
        CreateProcObject(1, args);
        HandleErrorStatus("Error report", SITH_SEV_WARNING);
     */

    char** args = calloc(3, sizeof (char*));
    args[0] = SITH_TEST_PROCESS;
    args[1] = calloc(SITH_MAXCH_INT, sizeof (char));

    // Do 4 iterations
    ProcessValue retval;
    for (int i = 0; i < 4; i++) {

        // Set the child argument
        snprintf(args[1], SITH_MAXCH_INT, "%d", i);

        // Start child process
        ProcessObject* proc = CreateProcObject(2, args);
        if (proc == NULL) {
            HandleErrorStatus("Could not create process");
            printf("["COLOR_RED"FAILED"COLOR_RESET"] Failure in creating process\n");
            free(args[1]);
            free(args);
            return -1;
        }

        // Wait for child process to exit
        int error = WaitForProcObject(proc, &retval);
        if (error) {
            HandleErrorStatus("Wait failed");
            printf("["COLOR_RED"FAILED"COLOR_RESET"] Failure in waiting process termination\n");
            free(args[1]);
            free(args);
            return -1;
        }

        // Inspect return value
#ifdef __unix__
        if (i == 3) {
            if (retval != SIGINT)
                printf("["COLOR_RED"FAILED"COLOR_RESET"] Process iteration %d failed, returned %d\n", i + 1, retval);
        }
        else if (retval != i)
            printf("["COLOR_RED"FAILED"COLOR_RESET"] Process iteration %d failed, returned %d\n", i + 1, retval);
#elif defined _WIN32
        if ((int) retval != i)
            printf("["COLOR_RED"FAILED"COLOR_RESET"] Process iteration %d failed, returned %lu\n", i + 1, retval);
#endif

    }

    printf("["COLOR_GREEN"OK"COLOR_RESET"] Process test passed\n");
    free(args[1]);
    free(args);
    return 0;
}

void f(const void* arg) {
    printf("%s\n", (const char*) arg);
}

int test_list() {
    LinkedList* l = CreateLinkedList();

    AppendToList(l, "abc");
    AppendToList(l, "def");

    PopHeadFromList(l);
    if (GetLinkedListSize(l) != 1) {
        printf("["COLOR_RED"FAILED"COLOR_RESET"] Failed at first head pop");
        return -1;
    }
    EmptyLinkedList(l);
    if (GetLinkedListSize(l) != 0) {
        printf("["COLOR_RED"FAILED"COLOR_RESET"] Failed in emptying list");
        return -1;
    }

    char* str = malloc(5 * sizeof (char*));

    memcpy(str, "five", 5);

    AppendToList(l, str);

    if (str != PopHeadFromList(l)) {
        printf("["COLOR_RED"FAILED"COLOR_RESET"] Failed at second head pop");
        return -1;
    }


    DestroyLinkedList(l);

    if (errno != 0) {
        printf("["COLOR_RED"FAILED"COLOR_RESET"] Failed destroying list: %d", errno);
        perror("");
        return -1;
    }
    errno = 0;
    printf("["COLOR_GREEN"OK"COLOR_RESET"] List test passed\n");

    return 0;
}

int main(void) {

    test_list();
    test_heap_string(); // Requires list
    test_walker(); // Requires heap_string
    test_process(); // Requires sync

    test_getter();
    test_setter();
    test_split();
// Relies on user input timing
    //test_pool();

    //test_file();

    //test_splitter();

    return 0;
}
