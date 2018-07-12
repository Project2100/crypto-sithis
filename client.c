/*
 * File:   client.c
 * Author: llde
 * Author: Project2100
 * Brief:  Client source code
 *
 * Created on 05 Aug 2017, 18:19
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>

#include "error.h"
#include "net.h"
#include "multi.h"

#include "string.h"
#include "arguments.h"
#include "log.h"
#include "proto.h"
#include "default.h"
#include "sync.h"
#include "list.h"
#include "string_heap.h"

//------------------------------------------------------------------------------
// ARGUMENTS

#define SITH_CLI_OPTNUM 8
#define SITH_CLI_TITLE "Crypto-Sithis, client application"
#define SITH_CLI_OPTIONS (Option[]) {\
    {'h', "",               0, SITH_OPT_FALSE,         "Show this help"},\
    {'a', "server_addr",    1, SITH_DEFAULT_ADDRESS,   "The server's IP address. Default is localhost"},\
    {'p', "server_port",    1, SITH_DEFAULT_PORT,      "The server's port. Default is 8888"},\
    {'L', "",               0, SITH_OPT_FALSE,         "Override the configuration file to localhost" }, \
    {'l', "",               0, SITH_OPT_FALSE,         "Single Command Execution LSTF"},\
    {'r', "",               0, SITH_OPT_FALSE,         "Single Command Execution LSTR"},\
    {'e', "",               2, SITH_OPT_EMPTY,         "Single Command Execution ENCR, take path and seed"},\
    {'d', "",               2, SITH_OPT_EMPTY,         "Single Command Execution DECR, take path and seed"},\
}

#define SITH_CLI_CFGPATH "./client.conf"
#define SITH_CLI_LOGPATH "./client.log"

//------------------------------------------------------------------------------
// UTILITY MACROS

#define SITH_MAXCH_CLIENTREQ 511


//------------------------------------------------------------------------------
// CLIENT FIELDS

ConnectionSocket* server;
LinkedList* messageQueue;

CondVar* signaller;
LockObject* sigLock;

void printCommand(const void* command) {
    printf("%s\n", (const char*) command);
}


//------------------------------------------------------------------------------
// COMMUNICATION BODY

ThreadValue SITH_THREAD_CALLCONV communicationBody(void* arg) {

    (void) arg;
    int longmessage = 0;
    char* command, *response;

    while (1) {

        // Wait for a command to be enqueued to the list
        DoLockObject(sigLock);
        while (GetLinkedListSize(messageQueue) == 0) {
            WaitConditionVariable(signaller, sigLock);
        }

        // Extract command and release lock on command list
        command = (char*) PopHeadFromList(messageQueue);
        DoUnlockObject(sigLock);


        // Check if user wants to exit
        if (command == NULL) {
            CloseConnection(server);
            return SITH_RV_ZERO;
        }

        // Send the command to the server
        if (SendToPeer(server, command) == -1) {
            HandleErrorStatus("Failed to send command to server");
            CloseConnection(server);
            return SITH_RV_ONE;
        }

        //Command is sent, we shall deallocate the string if it was an encr/decr
        // The other commands are just macro aliases
        if (CLIENT_REQUEST(command, SITH_PROTO_ENCRYPT) || CLIENT_REQUEST(command, SITH_PROTO_DECRYPT)) {

            // Log the encryption-decryption command
            HeapString* s = CreateHeapString(command);
            HeapStringPrepend(s, " ");
            time_t now = time(NULL);
            HeapStringPrepend(s, ctime(&now));
            if (WriteLog(SITH_CLI_LOGPATH, s))
                HandleErrorStatus("Could not write log file");
            DisposeHeapString(s);

            free(command);
        }

        // Handle server response
        // It may be nice to redisplay correctly the user input after receiving a response.
        //However we have no access on the raw input before entering a newline or a carrige-return in ICANON mode
        //that is  the default. We will need RAW mode for this.
resp:
        switch (ReceiveFromPeer(server, &response)) {
            case -1:
                // CLI: Clear line
                printf("\r");
                HandleErrorStatus("Failed to receive response from server");
                CloseConnection(server);
                free(response);
                // Server is not responding, close client
                exit(EXIT_FAILURE);
            case 0:
                // CLI: Clear line
                printf("\r");
                printf("Connection closed by server\n");
                CloseConnection(server);
                free(response);
                // Server is not responding, close client
                exit(EXIT_FAILURE);
            default:

                // CLI: Clear line
                printf("\r");

                // See if we are still in long-message mode
                if (longmessage == 1) {
                    if (SERVER_RESPONSE(response, SITH_PROTO_MOREEND)) {
                        longmessage = 0;
                        free(response);
                        break;
                    }
                    else {
                        printf("%s", response);
                        free(response);
                        // CLI: Prompt
                        printf("> ");
                        goto resp;
                    }
                }

                if (SERVER_RESPONSE(response, SITH_PROTO_SUCCESS)) {
                    printf("Operation successful: %s\n", response + SITH_MAXCH_PROTORESP);
                    fflush(stdout);
                }
                else if (SERVER_RESPONSE(response, SITH_PROTO_INVALID)) {
                    printf("Bad request issued: %s\n", response + SITH_MAXCH_PROTORESP);
                    fflush(stdout);
                }
                else if (SERVER_RESPONSE(response, SITH_PROTO_FAILURE)) {
                    printf("Operation failed: %s\n", response + SITH_MAXCH_PROTORESP);
                    fflush(stdout);
                }
                else if (SERVER_RESPONSE(response, SITH_PROTO_SERVBUSY)) {
                    printf("Server is too busy: %s\n", response + SITH_MAXCH_PROTORESP);
                    fflush(stdout);
                }
                else if (SERVER_RESPONSE(response, SITH_PROTO_NOTIMPL)) {
                    printf("Operation not implemented: %s\n", response + SITH_MAXCH_PROTORESP);
                    fflush(stdout);
                }
                else if (SERVER_RESPONSE(response, SITH_PROTO_MOREOUT)) {
                    longmessage = 1;
                    free(response);
                    goto resp;
                }
                else {
                    printf("Could not interpret server response: %s\n", response);
                    fflush(stdout);
                }

                free(response);
        }

        // CLI: Prompt
        printf("> ");
        fflush(stdout);
    }

    // We should NEVER get here
    HandleErrorStatus("WARNING: Connection handler thread broke out of the loop");
    return SITH_RV_ONE;
}

int SingleExecutionMode(int mode, ThreadObject* comm, char* opt1, char* opt2) {
    if (mode == 0) {
        DoLockObject(sigLock);
        AppendToList(messageQueue, SITH_PROTO_LIST);
        DoUnlockObject(sigLock);
        NotifyConditionVariable(signaller);
    }
    else if (mode == 1) {
        DoLockObject(sigLock);
        AppendToList(messageQueue, SITH_PROTO_LISTREC);
        DoUnlockObject(sigLock);
        NotifyConditionVariable(signaller);
    }
    else if (mode == 2) {
        HeapString* comm_temp = CreateHeapString(SITH_PROTO_ENCRYPT);
        if (comm_temp == NULL) {
            HandleErrorStatus("Could not execute the command");
            return SITH_RET_ERR;
        }
        HeapStringAppend(comm_temp, opt1);
        HeapStringAppend(comm_temp, " ");
        HeapStringAppend(comm_temp, opt2);
        DoLockObject(sigLock);
        AppendToList(messageQueue, HeapStringInner(comm_temp));
        DoUnlockObject(sigLock);
        NotifyConditionVariable(signaller);
    }
    else if (mode == 3) {
        HeapString* comm_temp = CreateHeapString(SITH_PROTO_DECRYPT);
        if (comm_temp == NULL) {
            HandleErrorStatus("Could not execute the command");
            return SITH_RET_ERR;
        }
        HeapStringAppend(comm_temp, opt1);
        HeapStringAppend(comm_temp, " ");
        HeapStringAppend(comm_temp, opt2);
        DoLockObject(sigLock);
        AppendToList(messageQueue, HeapStringInner(comm_temp));
        DoUnlockObject(sigLock);
        NotifyConditionVariable(signaller);
    }
    DoLockObject(sigLock);
    AppendToList(messageQueue, NULL);
    DoUnlockObject(sigLock);
    NotifyConditionVariable(signaller);
    WaitForThread(comm, NULL);
    return SITH_RET_OK;
}



//------------------------------------------------------------------------------
// CLIENT MAIN

void terminateWSA(void) {
    SocketAPIDestroy();
}

int main(int argc, char** argv) {

    InitArguments(SITH_CLI_OPTIONS, SITH_CLI_OPTNUM, SITH_CLI_TITLE, SITH_CLI_CFGPATH, NULL);

    // Configuration fields
    if (argc > 1) {
        if (ParseArguments(argc, argv) == SITH_RET_ERR) {
            HandleErrorStatus("Failed reading arguments");
            return EXIT_FAILURE;
        }
    }

    unsigned short show_help = 0;
    GetOptionBool('h', 0, &show_help);
    if (show_help == 1) {
        PrintHelp();
        return EXIT_SUCCESS;
    }

    // Address
    char address[SITH_MAXCH_IPV4 + 1] = {0};
    unsigned short force_local = 0;
    GetOptionBool('L', 0, &force_local);
    if (force_local == 0) {
        GetOptionString('a', 1, address);
    }
    else {
        printf("Ignore configuration file address. Defaulting to localhost\n");
        strcpy(address, SITH_DEFAULT_ADDRESS);
    }

    // Port
    unsigned short port = 0;
    if (GetOptionUShort('p', 1, &port) < 0) {
        HandleErrorStatus("Bad port specified");
        return EXIT_FAILURE;
    }

    // Options have been read, start the client
    SocketAPIInit();
    atexit(terminateWSA);

    // Report configuration
    printf("\n--Crypto Sithis Client--\n");
    printf("Connecting to %s:%hu... ", address, port);
    fflush(stdout);

    // Connect to server
    server = ConnectToServer(address, port);
    if (server == NULL) {
        printf("failed.\n");
        HandleErrorStatus("Socket failure");
        return EXIT_FAILURE;
    }

    // Receive server response
    char* response;
    if (ReceiveFromPeer(server, &response) <= 0) {
        printf("failed.\n");
        HandleErrorStatus("Could not get confirmation message from server");
        return EXIT_FAILURE;
    }
    if (SERVER_RESPONSE(response, SITH_PROTO_SERVBUSY)) {
        free(response);
        CloseConnection(server);
        printf("failed\n");
        HandleErrorStatus("Connection closed by server: busy");
        return EXIT_FAILURE;
    }
    else if (!(SERVER_RESPONSE(response, SITH_PROTO_ACCEPTED))) {
        free(response);
        CloseConnection(server);
        printf("failed\n");
        HandleErrorStatus("Could not interpret server response, exiting...");
        return EXIT_FAILURE;
    }
    free(response);
    printf("OK.\n");

    // Build list locks
    signaller = CreateConditionVar();
    if (signaller == NULL) {
        HandleErrorStatus("Cannot instantiate locking resources");
        return EXIT_FAILURE;
    }
    sigLock = CreateLockObject();
    if (sigLock == NULL) {
        HandleErrorStatus("Cannot instantiate locking resources");
        return EXIT_FAILURE;
    }

    // Allocate the command list and spin the communicator
    messageQueue = CreateLinkedList();
    if (messageQueue == NULL) {
        HandleErrorStatus("Could not allocate command list");
        return EXIT_FAILURE;
    }
    ThreadObject* comm = SpawnThread(communicationBody, NULL);
    if (comm == NULL) {
        HandleErrorStatus("Comm thread spawn failed");
        return EXIT_FAILURE;
    }

    // Look for single execution modes
    unsigned short list_ex = 0, listrec_ex = 0;
    GetOptionBool('l', 0, &list_ex);
    GetOptionBool('r', 0, &listrec_ex);
    if (list_ex == 1) {
        return SingleExecutionMode(0, comm, NULL, NULL);
    }
    if (listrec_ex == 1) {
        return SingleExecutionMode(1, comm, NULL, NULL);
    }
    char seed[SITH_MAX_VALUE_LEN] = {0};
    char path[SITH_MAX_VALUE_LEN] = {0};
    GetOptionString('e', 2, seed);
    GetOptionString('e', 1, path);
    if (strcmp("", seed) && strcmp("", path) != 0) {
        return SingleExecutionMode(2, comm, path, seed);
    }
    GetOptionString('d', 2, seed);
    GetOptionString('d', 1, path);
    if (strcmp("", seed) && strcmp("", path) != 0) {
        return SingleExecutionMode(3, comm, path, seed);
    }

    // No single commands, going interactive
    printf("Type \"help\" to display available commands\n");

    // Main loop
    char command[SITH_MAXCH_CLIENTREQ + 1];
    while (1) {

        memset(command, 0, SITH_MAXCH_CLIENTREQ + 1);
        printf("> ");

        if (fgets(command, SITH_MAXCH_CLIENTREQ, stdin) == NULL) {
            // Check for end of stream
            if (feof(stdin)) {
                CloseConnection(server);
                return EXIT_SUCCESS;
            }
            else {
                // We have a console error here...
                HandleErrorStatus("Failed reading input");
                return EXIT_FAILURE;
            }
        }

        // Read the user command
        // Lowercase the command only (till 1st space!)
        for (char* p = command; *p != ' ' && *p != '\0' && *p != '\n' && *p != '\t'; ++p) *p = tolower(*p);

        // Check if user wants to exit
        if (CLIENT_COMMAND(command, SITH_CMD_EXIT) || CLIENT_COMMAND(command, SITH_CMD_EXIT2)) {

            // Schedule the exit command
            DoLockObject(sigLock);

            if (GetLinkedListSize(messageQueue) > 0) {
                printf("Finishing queued commands...\n");
            }
            AppendToList(messageQueue, NULL);

            // Release the lock, let the comm body finish its command list
            DoUnlockObject(sigLock);
            NotifyConditionVariable(signaller);

            // Once the body reaches the exit command, it will return
            // NOTE: All commands scheduled beforehand must terminate
            WaitForThread(comm, NULL);

            // Comm body has returned, we are alone. Kill everything
            DestroyLinkedList(messageQueue);
            DestroyConditionVar(signaller);
            DestroyLockObject(sigLock);

            // We can go off...
            printf("Exiting...\n");
            return EXIT_SUCCESS;
        }
        else if (CLIENT_COMMAND(command, SITH_CMD_HELP)) {
            printf("\nCommands:\n"
                    "help:     Prints this help message\n"
                    "exit:     Closes the client\n"
                    "quit:     Same as \"exit\"\n"
                    "queue:    Displays the list of commands yet to be sent\n"
                    "clear:    Empties the list of commands to be sent\n"
                    "list:     Queries the server for the files in its current folder\n"
                    "listrec:  Same as \"list\", but recursively lists subfolders\n"
                    "encrypt <filename> <seed>: Instructs the server to encrypt the file specified by <filename> using <seed> for the encryption\n"
                    "decrypt <filename> <seed>: Same as \"encrypt\", filename must end with _enc\n\n");
        }
        else if (CLIENT_COMMAND(command, SITH_CMD_QUEUE)) {
            DoLockObject(sigLock);
            if (GetLinkedListSize(messageQueue) == 0) {
                printf("There are no pending commands\n");
            }
            else {
                printf("Pending commands:\n");
                ApplyOnList(messageQueue, printCommand);
            }
            DoUnlockObject(sigLock);
            NotifyConditionVariable(signaller);
        }
        else if (CLIENT_COMMAND(command, SITH_CMD_CLEAR)) {
            DoLockObject(sigLock);
            EmptyLinkedList(messageQueue);
            DoUnlockObject(sigLock);
            printf("Commands cleared.\n");
            // Notifying is useless: list is empty
        }
        else if (CLIENT_COMMAND(command, SITH_CMD_LIST)) {
            DoLockObject(sigLock);
            AppendToList(messageQueue, SITH_PROTO_LIST);
            DoUnlockObject(sigLock);
            NotifyConditionVariable(signaller);
        }
        else if (CLIENT_COMMAND(command, SITH_CMD_LISTREC)) {
            DoLockObject(sigLock);
            AppendToList(messageQueue, SITH_PROTO_LISTREC);
            DoUnlockObject(sigLock);
            NotifyConditionVariable(signaller);
        }
        else if (CLIENT_COMMAND(command, SITH_CMD_ENCRYPT)) {
            HeapString* comm_temp = CreateHeapString(command);
            if (comm_temp == NULL) {
                HandleErrorStatus("Could not execute the command");
                continue;
            }
            HeapString* head = HeapStringSplitAtCharFirst(comm_temp, ' ');
            DisposeHeapString(head);
            HeapStringPrepend(comm_temp, SITH_PROTO_ENCRYPT);
            DoLockObject(sigLock);
            AppendToList(messageQueue, HeapStringInner(comm_temp));
            DoUnlockObject(sigLock);
            NotifyConditionVariable(signaller);

        }
        else if (CLIENT_COMMAND(command, SITH_CMD_DECRYPT)) {
            HeapString* comm_temp = CreateHeapString(command);
            if (comm_temp == NULL) {
                HandleErrorStatus("Could not execute the command");
                continue;
            }
            HeapString* head = HeapStringSplitAtCharFirst(comm_temp, ' ');
            DisposeHeapString(head);
            HeapStringPrepend(comm_temp, SITH_PROTO_DECRYPT);
            DoLockObject(sigLock);
            AppendToList(messageQueue, HeapStringInner(comm_temp));
            DoUnlockObject(sigLock);
            NotifyConditionVariable(signaller);
        }
        else {
            printf("Command unrecognized. Try typing \"help\" to display available commands\n");
        }

    }

    // We should NEVER get here
    HandleErrorStatus("FATAL: main thread broke out of the loop");
    return EXIT_FAILURE;
}
