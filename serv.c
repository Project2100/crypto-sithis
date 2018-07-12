/*
 * File:   serv.c
 * Author: Project2100
 * Brief:  Server source code
 *
 * Created on 23 May 2017, 18:59
 * 
 */

#include <stdio.h>
#include <stdlib.h>

#include "file.h"
#include "error.h"
#include "net.h"
#include "multi.h"
#include "filewalker.h"
#include "string_heap.h"

#ifdef __unix__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#endif

#include "pool.h"
#include "string.h"
#include "arguments.h"

#include "proto.h"
#include "default.h"
#include "list.h"
#include "sync.h"
#include "crypto.h"


//------------------------------------------------------------------------------
// ARGUMENTS

#define SITH_SERV_OPTNUM 7
#define SITH_SERV_TITLE "Crypto-Sithis, server application"
#define SITH_SERV_OPTIONS (Option[]) {\
    {'h', "",                       0, SITH_OPT_FALSE,               "Show this help"},\
    {'a', "server_addr",            1, SITH_DEFAULT_ADDRESS,         "The server's IP address. Default is localhost"},\
    {'p', "server_port",            1, SITH_DEFAULT_PORT,            "The server's port. Default is 8888"},\
    {'L', "",                       0, SITH_OPT_FALSE,               "Listen to localhost. Override the configuration file"},\
    {'c', "current_root_dir" ,      1, SITH_DEFAULT_SERVROOT,        "Set the server's root directory"},\
    {'u', "max_client_connect",     1, SITH_DEFAULT_SERVMAXCLI,      "Set maximum number of concurrent clients"},\
    {'I', "",                       0, SITH_OPT_FALSE,               "Do not daemonize (no effect on Windows)"}\
}

#define SITH_SERV_CFGPATH "server.conf"
#define SITH_SERV_LOGPATH "./server.log"
#define SITH_SERV_ERRPATH "./server_err.log"

#define SITH_SERVOPT_HELP 0
#define SITH_SERVOPT_ADDRESS 1
#define SITH_SERVOPT_LOCAL 2
#define SITH_SERVOPT_PORT 3
#define SITH_SERVOPT_ROOT 4
#define SITH_SERVOPT_CLIENTS 5
#define SITH_SERVOPT_INTERACTIVE 6

//------------------------------------------------------------------------------
// RETURN VALUES

// NOTE: ProcesValues from 0 to 255 are reserved for the default values defined in multi.h
#define SITH_ENDECFAIL_INVAL 0x0100
#define SITH_ENDECFAIL_STR 0x0101
#define SITH_ENDECFAIL_NOMEM 0x0102
#define SITH_ENDECFAIL_SYS 0x0103


//------------------------------------------------------------------------------
// UTILITY MACROS

#define SITH_ENCRSFX "_enc"
#define SITH_MAXCH_ENCRSFX 4
#define SITH_SERV_BACKLOG 32
// Asserting that server is executed in its folder, and that crypto is located in the same folder
#ifdef _WIN32
#define SITH_FILENAME_CRYPTO "crypto.exe"
#define SITH_FILENAME_CONFIG "config.txt"
#elif defined __unix__
#define SITH_FILENAME_CRYPTO "crypto"
#define SITH_FILENAME_CONFIG "config.txt"
#endif


//------------------------------------------------------------------------------
// SERVER FIELDS

ThreadObject* listenerThread;
ThreadPool* clients;
char* configPathName;
char* cryptoPathName;
ListenerSocket* listener;

#ifdef __unix__
volatile sig_atomic_t servChange = 0;
volatile sig_atomic_t servFailure = 0;
struct sockaddr_in addressBuffer = {
    .sin_addr =
    {0},
    .sin_family = AF_INET,
    .sin_port = 0,
    .sin_zero =
    {0}
};
#endif

typedef struct bitfield_mask {
    unsigned int changed_port : 1;
    unsigned int changed_address : 1;
    unsigned int changed_max_clients : 1;
    unsigned int changed_root_dir : 1;
    //The compiler will probably inject 3 byte padding here . Test in case insert a manual padding to remain consistent
} BitFieldMask;

void maskerer(void* bitfield_mask, unsigned short* opt, size_t size) {
    if (bitfield_mask == NULL) return;
    if (size != SITH_SERV_OPTNUM) {
        HandleErrorStatus("Something very wrong happened");
        return;
    }
    BitFieldMask* mask = (BitFieldMask*) bitfield_mask;
    if (opt[SITH_SERVOPT_ADDRESS] == 1) mask->changed_address = 1;
    if (opt[SITH_SERVOPT_PORT] == 1) mask->changed_port = 1;
    if (opt[SITH_SERVOPT_ROOT] == 1) mask->changed_root_dir = 1;
    if (opt[SITH_SERVOPT_CLIENTS] == 1) mask->changed_max_clients = 1;
}
//------------------------------------------------------------------------------
// ENCODE-DECODE FUNCTION

int EndecFile(char* request, int doEncrypt, ProcessValue* outcome) {
    if (request == NULL || outcome == NULL) {
        return SITH_ENDECFAIL_INVAL;
    }

    // Build a string around the request
    HeapString* sourcePath = CreateHeapString(request);
    if (sourcePath == NULL) {
        HandleErrorStatus("Error while creating request handler");
        return SITH_ENDECFAIL_STR;
    }

    // Extract seed
    HeapStringTrim(sourcePath);
    HeapString* seed = HeapStringSplitAtCharLast(sourcePath, ' ');
    if (seed == NULL) {
        HandleErrorStatus("Not enough arguments");
        DisposeHeapString(sourcePath);
        return SITH_ENDECFAIL_STR;

    }
    HeapStringTruncate(sourcePath, HeapStringLength(seed));
    HeapStringTrim(sourcePath);
    HeapStringRemoveEndTokens(seed);

    // Duplicate for target pathname construction
    HeapString* targetPath = HeapStringClone(sourcePath);

    // Unquote
    int quoted = 0;
    if (HeapStringCharAt(targetPath, 0) == '"' && HeapStringCharAt(targetPath, HeapStringLength(sourcePath) - 1) == '"') {
        quoted = 1;
        HeapStringRemoveStart(targetPath, 1);
        HeapStringTruncate(targetPath, 1);
    }

    // Build target path
    if (doEncrypt) {
        HeapStringAppend(targetPath, SITH_ENCRSFX);
    }
    else {
        if (HeapStringCheckSuffix(targetPath, SITH_ENCRSFX) == SITH_RET_ERR) {
            fprintf(stderr, "Specified file is not encoded: %s\n", HeapStringGetRaw(targetPath));
            DisposeHeapString(targetPath);
            DisposeHeapString(sourcePath);
            DisposeHeapString(seed);
            return SITH_ENDECFAIL_INVAL;
        }
        HeapStringTruncate(targetPath, SITH_MAXCH_ENCRSFX);
    }

    // Requote
    if (quoted) {
        HeapStringPrepend(targetPath, "\"");
        HeapStringAppend(targetPath, "\"");
    }


    // Start writing the command line
    char** argv = calloc(5, sizeof (char*));
    if (argv == NULL)
        return SITH_ENDECFAIL_NOMEM;
    argv[0] = cryptoPathName;
    argv[1] = HeapStringInner(sourcePath);
    argv[2] = HeapStringInner(targetPath);
    argv[3] = HeapStringInner(seed);

    // DIRECT CALL, make the client wait for us
    ProcessObject* proc = CreateProcObject(4, argv);

    // Free strings immediately, they've done their purpose
    free(argv[1]);
    free(argv[2]);
    free(argv[3]);
    free(argv);
    if (proc == NULL) {
        HandleErrorStatus("Could not launch crypto");
        return SITH_ENDECFAIL_SYS;
    }

    // Wait for crypto to terminate
    int error = WaitForProcObject(proc, outcome);
    if (error) {
        HandleErrorStatus("Error waiting for crypto");
        return SITH_ENDECFAIL_SYS;
    }
    
    return 0;
}


//------------------------------------------------------------------------------
// CLIENT CONNECTION TASK

typedef SITH_TASKARG struct {
    ConnectionSocket* peerSocket;
    char* peerAddress;
} ConnTaskArg;

SITH_TASKBODY int clientTask(void* arg) {
    ConnTaskArg* connInfo = (ConnTaskArg*) arg;

    // Send confirmation to client to start sending messages
    if (SendToPeer(connInfo->peerSocket, SITH_PROTO_ACCEPTED) == SITH_RET_ERR) {
        HandleErrorStatus("Failed to synchronize with client");
        return -1;
    }

    printf("Accepted connection from %s\n", connInfo->peerAddress);
    fflush(stdout);
    // Error status is set here
    char* request;
    ProcessValue ret = 0;
    int isEncryptionRequest = 0;
    while (1) switch (ReceiveFromPeer(connInfo->peerSocket, &request)) {

            case -1: // Socket failure
                fprintf(stderr, "[%s] ", connInfo->peerAddress);
                HandleErrorStatus("Connection lost");

                CloseConnection(connInfo->peerSocket);
                free(connInfo->peerAddress);
                free(connInfo);
                return 0;

            case 0: // Socket closed
                printf("[%s] Client closed the connection.\n", connInfo->peerAddress);
                fflush(stdout);

                CloseConnection(connInfo->peerSocket);
                free(connInfo->peerAddress);
                free(connInfo);
                return 0;

            default: // We actually got a message
                printf("[%s] Message received: %s\n", connInfo->peerAddress, request);

                // Directory listing option
                if (CLIENT_REQUEST(request, SITH_PROTO_LIST)) {
                    HeapString* output = CreateHeapString("");
                    if (output == NULL) {
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE);
                        continue;
                    }
                    SithWalker* walk = InitWalker(".");
                    if (walk == NULL) {
                        DisposeHeapString(output);
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE);
                        continue;
                    }
                    WalkInDir(walk, output);
                    if (output != NULL) {
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_MOREOUT);
                        SendToPeer(connInfo->peerSocket, HeapStringGetRaw(output));
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_MOREEND);
                    }
                    DisposeHeapString(output);
                    DisposeWalker(walk);
                }

                    // Recursive listing option
                else if (CLIENT_REQUEST(request, SITH_PROTO_LISTREC)) {
                    HeapString* output = CreateHeapString("");
                    if (output == NULL) {
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE);
                        continue;
                    }
                    SithWalker* walk = InitWalker(".");
                    if (walk == NULL) {
                        DisposeHeapString(output);
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE);
                        continue;
                    }
                    WalkInDirRecursive(walk, output);
                    if (output != NULL) {
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_MOREOUT);
                        SendToPeer(connInfo->peerSocket, HeapStringGetRaw(output));
                        SendToPeer(connInfo->peerSocket, SITH_PROTO_MOREEND);
                    }
                    DisposeHeapString(output);
                    DisposeWalker(walk);
                }
                    // Encrypt-Decrypt file option
                else if ((isEncryptionRequest = CLIENT_REQUEST(request, SITH_PROTO_ENCRYPT)) || CLIENT_REQUEST(request, SITH_PROTO_DECRYPT)) {

                    // Call the worker function and send a response accordingly
                    switch (EndecFile(request + SITH_MAXCH_PROTOCMD, isEncryptionRequest, &ret)) {
                        case 0:
                            // Process returned, read exit code
                            switch (ret) {
                                case 0:
                                    // File has been correctly encrypted
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_SUCCESS"OK");
                                    break;
                                case SITH_FAILCRYPTO_404:
                                    // Encountered error while handling files
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_INVALID"File not found");
                                    break;
                                case SITH_FAILCRYPTO_SEED:
                                    // Invalid seed
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_INVALID"Seed is malformed");
                                    break;
                                case SITH_FAILCRYPTO_ARG:
                                    // Bad argument count
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_INVALID"Wrong number of arguments");
                                    break;
                                case SITH_FAILCRYPTO_NOTREG:
                                    // Invalid pathname
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_INVALID"Path does not denote a regular file");
                                    break;
                                case SITH_FAILCRYPTO_LOCKED:
                                    // Source file is locked
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"This file is currently locked, try again later.");
                                    break;
                                case SITH_FAILCRYPTO_FILE:
                                    // File error
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"File error");
                                    break;
                                case SITH_FAILCRYPTO_ENDEC:
                                    // File partially encrypted
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"File has been partially encrypted");
                                    break;
                                case SITH_FAILCRYPTO_RELEASE:
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"Error while releasing resources");
                                    break;
                                case SITH_FAILCRYPTO_NOMEM:
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"Not enough memory for encryption");
                                    break;
                                default:
                                    printf("Unexpected return code from crypto: %"SITH_STRFMT_PROCVALUE"\n", ret);
                                    SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"Unexpected error in encryption");
                                    break;
                            }
                            break;
                        case SITH_ENDECFAIL_INVAL:
                        case SITH_ENDECFAIL_STR:
                            // Command is malformed
                            SendToPeer(connInfo->peerSocket, SITH_PROTO_INVALID"The received parameters are malformed");
                            break;
                        case SITH_ENDECFAIL_SYS:
                        case SITH_ENDECFAIL_NOMEM:
                            // Internal failure (pool saturated) (transient)
                            SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"Not enough memory for encryption task scheduling");
                            break;
                        default:
                            SendToPeer(connInfo->peerSocket, SITH_PROTO_FAILURE"Unexpected error");
                    }
                }

                    // Message unrecognized
                else {
                    printf("[%s] Malformed request\n", connInfo->peerAddress);
                    SendToPeer(connInfo->peerSocket, SITH_PROTO_INVALID);
                }

                // Do free the request
                free(request);
        }

    //We should never get here, but just in case...
    CloseConnection(connInfo->peerSocket);
    free(connInfo->peerAddress);
    free(connInfo);
    return -1;
}


//------------------------------------------------------------------------------
// LISTENER THREAD BODY

ThreadValue SITH_THREAD_CALLCONV listenerBody(void* arg) {

    (void) arg;

#ifdef __unix__
    // Leave only SIGUSR1 open for address updates
    sigset_t usr1Mask;
    sigfillset(&usr1Mask);
    sigdelset(&usr1Mask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &usr1Mask, NULL);
#endif

    printf("Server is listening...\n");
    fflush(stdout);

    // Listening loop
    int error;
    ConnectionSocket* peer;
    while (1) {
#ifdef __unix__
        // Log an eventual address change
        if (servChange) {

            if (servFailure) {
                HandleErrorStatus("Server failed updating\n");
                exit(EXIT_FAILURE);
            }
            printf("Server has changed address\n");
            servChange = 0;
        }
#endif

        // Accept connection
        peer = AcceptFromClient(listener);
        if (peer == NULL) {
#ifdef __unix__
            // Do check if we got interrupted by a SIGUSR1
            if (errno == EINTR) {
                errno = 0;
                continue;
            }
#endif
            HandleErrorStatus("Failed accepting connection");
            continue;
        }

        // Create connection info
        ConnTaskArg* connArg = calloc(1, sizeof (ConnTaskArg));
        if (connArg == NULL) {
            HandleErrorStatus("Could not allocate connection info");
            SendToPeer(peer, SITH_PROTO_FAILURE);
            CloseConnection(peer);
            continue;
        }

        // Set peer address
        error = GetPeerFullAddress(peer, &(connArg->peerAddress));
        if (connArg->peerAddress == NULL) {
            HandleErrorStatus("Could not get peer address");
            SendToPeer(peer, SITH_PROTO_FAILURE);
            free(connArg);
            CloseConnection(peer);
            continue;
        }

        connArg->peerSocket = peer;

        // Start communication loop with client

        error = ScheduleTask(clients, clientTask, connArg, 0);
        if (error) {
            HandleErrorStatus("Connection aborted");
            SendToPeer(peer, SITH_PROTO_SERVBUSY);
            free(connArg);
            CloseConnection(peer);
            continue;
        }
    }

    // No breaks expected in listening loop
    CloseListener(listener);
    return SITH_RV_ONE;
}


//------------------------------------------------------------------------------
// SIGNAL HANDLERS


#ifdef __unix__

void servUpdateSigHandler(int sig, siginfo_t* info, void* ucontext) {
    (void) ucontext;

    // Do check if we caught a signal of our own
    if (sig != SIGUSR1 || info->si_pid != GetSelfPID()) {
        return;
    }

    servChange = 1;

    // Bypass the whole net abstraction - we need to be AsyncSignalSafe

    // Close the old listener
    if (close(listener->descriptor)) {
        servFailure = 1;
        return;
    }

    // Open the socket
    if ((listener->descriptor = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        servFailure = 1;
        return;
    }

    // Bind it to the new address
    if (bind(listener->descriptor, (struct sockaddr*) &addressBuffer, sizeof (struct sockaddr_in))) {
        servFailure = 1;
        return;
    }

    // Relisten
    if (listen(listener->descriptor, listener->backlog)) {
        servFailure = 1;
        return;
    }
}

void emptySigHandler(int sig) {
    (void) sig;
}
#endif

//------------------------------------------------------------------------------
// SERVER MAIN

void terminateWSA(void) {
    SocketAPIDestroy();
}

int main(int argc, char* argv[]) {

    // Initialize process options
    //Read the configuration file or create a default one if donesn't exist
    InitArguments(SITH_SERV_OPTIONS, SITH_SERV_OPTNUM, SITH_SERV_TITLE, SITH_SERV_CFGPATH, &maskerer);
    // Then, apply CLI options
    if (argc > 1) {
        if (ParseArguments(argc, argv) == SITH_RET_ERR) {
            HandleErrorStatus("Error while parsing arguments");
            return EXIT_FAILURE;
        }
    }

    // If the help option is specified, display help and exit
    unsigned short show_help = 0, interactive = 0;
    GetOptionBool('h', 0, &show_help);
    if (show_help) {
        PrintHelp();
        return EXIT_SUCCESS;
    }

#ifdef __unix__
    GetOptionBool('I', 0, &interactive);
    if (interactive == 0) {

        // Daemonization:
        // - fork
        // - setsid
        // - fork
        // - chdir (don't, we actually operate on current directory)
        // - umask (0?)
        // - stdstreams
        int fd = fork();
        if (fd == -1) exit(EXIT_FAILURE);
        if (fd != 0) exit(EXIT_SUCCESS);

        if (setsid() == -1) return EXIT_FAILURE;

        int fd1 = fork();
        if (fd1 == -1) exit(EXIT_FAILURE);
        if (fd1 != 0) exit(EXIT_SUCCESS);

        umask(027);

        // Cut stdin
        close(0);
        open("/dev/null", S_IRUSR);
        // Redirect stdout and stderr to two separate file for logging purposes
        close(1);
        if (open(SITH_SERV_LOGPATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR) == -1)
            return SITH_RET_ERR;
        close(2);
        if (open(SITH_SERV_ERRPATH, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR) == -1)
            return SITH_RET_ERR;
    }
    else {
        printf("Starting in Interactive Debug Mode\n");
        fflush(stdout);
    }

#elif defined _WIN32
    // TDNX May implement WINAPI services in the future...
    (void) interactive;

#endif

    // Set pathnames of crypto executable and configuration file
    configPathName = calloc(SITH_MAXCH_PATHNAME + 1, sizeof (char));
    cryptoPathName = calloc(SITH_MAXCH_PATHNAME + 1, sizeof (char));

    GetWorkingDirectory(configPathName, SITH_MAXCH_PATHNAME - strlen(SITH_FILENAME_CONFIG));
    configPathName[strlen(configPathName)] = SITH_NAMESEP;
    memcpy(cryptoPathName, configPathName, SITH_MAXCH_PATHNAME);

    memcpy(configPathName + strlen(configPathName), SITH_FILENAME_CONFIG, strlen(SITH_FILENAME_CONFIG));
    memcpy(cryptoPathName + strlen(cryptoPathName), SITH_FILENAME_CRYPTO, strlen(SITH_FILENAME_CRYPTO));

    char address[SITH_MAXCH_IPV4 + 1] = {0};
    unsigned short force_local = 0;
    GetOptionBool('L', 0, &force_local);
    if (force_local == 0) {

        GetOptionString('a', 1, address);
    }
    else {
        //printf("Ignore configuration file address. Defaulting to localhost\n");
        strcpy(address, SITH_DEFAULT_ADDRESS);
    }
    unsigned short port = 0;
    if (GetOptionUShort('p', 1, &port) < 0) {
        HandleErrorStatus("Bad port specified");
        return EXIT_FAILURE;
    }

    char root[SITH_MAX_VALUE_LEN] = {0};
    GetOptionString('c', 1, root); // TDNX sizes;

    unsigned int maxTasks = 0;
    GetOptionUInt('n', 1, &maxTasks);

    unsigned int maxClients = 0;
    GetOptionUInt('u', 1, &maxClients);


    // ====================
    // If we got here, then options have been read correctly, server shall start
    //
    // PRE-INIT ################################################################

    // Report configuration
    printf("\n--Crypto Sithis Server--\n");
    printf("Server address: %s:%hu\n", address, port);
    printf("Root folder: %s\n", root);
    printf("Max clients: %u\n", maxClients);
    printf("Max tasks: %u\n\n", maxTasks);

    // Change root directory if requested
    if (strcmp(root, SITH_DEFAULT_SERVROOT) != 0) {
        if (SetWorkingDirectory(root)) {
            HandleErrorStatus("Could not set root directory");
            printf("Root will default to current working directory");
            SetOptionString('c', 0, SITH_DEFAULT_SERVROOT);
        }
    }

#ifdef _WIN32
    // Init WSA before listening
    if (SocketAPIInit()) {
        HandleErrorStatus("Failed to load WinSock");
        // Can't do anything without sockets, bail out
        exit(EXIT_FAILURE);
    }
    atexit(terminateWSA);
#elif defined __unix__

    // Block all signals before spawning other threads (and pools) from here
    sigset_t muteMask;
    sigfillset(&muteMask);
    sigset_t originalMask = {
        {0}
    };
    errno = pthread_sigmask(SIG_SETMASK, &muteMask, &originalMask);
    if (errno) {
        // Can't ensure program integrity, advise the user
        HandleErrorStatus("WARNING: Could not block signals, program may become unstable");
    }

#endif


    // INIT ####################################################################

    // [UNIX] All signals are blocked, start the actual server logic
    //   so that all threads spawned here inherit the "block-all" sigmask 
    clients = CreateThreadPool("CS_clients", maxClients);
    if (clients == NULL) {
        HandleErrorStatus("Could not create connection pool");
        exit(EXIT_FAILURE);
    }

    // ACTIVATION POINT
    // Open server socket
    listener = CreateServerSocket(address, port, SITH_SERV_BACKLOG);
    if (listener == NULL) {
        HandleErrorStatus("FATAL: Failed to create server socket");
        exit(EXIT_FAILURE);
    }

    // Start listener thread
    listenerThread = SpawnThread(listenerBody, NULL);

    // POST-INIT ###############################################################
#ifdef _WIN32
    // Stun this thread, we need it alive to avoid process termination
    while (1) Sleep(INFINITE);

#elif defined __unix__
    // Restore default signal mask
    // From now on, main tread will act as the process' signal handler and
    // change the process' config accordingly
    sigset_t actualMask = originalMask;
    // Block SIGUSR1, let the listener react to it
    sigaddset(&actualMask, SIGUSR1);
    pthread_sigmask(SIG_SETMASK, &actualMask, NULL);


    // Change signal dispositions

    // SIGHUP handler is empty, we handle configuration update here
    struct sigaction hupaction;
    hupaction.sa_handler = emptySigHandler;
    sigfillset(&hupaction.sa_mask);
    hupaction.sa_flags = 0;
    sigaction(SIGHUP, &hupaction, (struct sigaction*) NULL);

    // SIGUSR1 will be raised here and caught by the listener
    // Use the enhanced function prototype to check the signal PID
    struct sigaction usr1action;
    usr1action.sa_sigaction = servUpdateSigHandler;
    sigfillset(&usr1action.sa_mask);
    usr1action.sa_flags = SA_SIGINFO;
    sigaction(SIGUSR1, &usr1action, (struct sigaction*) NULL);

    // Ignore SIGPIPE, we handle send failures on our own
    struct sigaction pipeaction;
    pipeaction.sa_handler = SIG_IGN;
    sigfillset(&pipeaction.sa_mask);
    pipeaction.sa_flags = 0;
    sigaction(SIGPIPE, &pipeaction, (struct sigaction*) NULL);

    // Set up the SIGHUP mask
    sigset_t hupset;
    sigemptyset(&hupset);
    sigaddset(&hupset, SIGHUP);
    sigaddset(&hupset, SIGCHLD);
    siginfo_t signalInfo;
    char newAddr[SITH_MAXCH_IPV4 + 1];
    unsigned short newPort;
    while (1) {

        // Wait for SIGHUP
        if (sigwaitinfo(&hupset, &signalInfo) == -1) {
            HandleErrorStatus("Unexpected error on signal wait");
            // We're not in a good place, just bail out
            exit(EXIT_FAILURE);
        }

        // Quick hack: ignore SIGCHLD
        if (signalInfo.si_signo == SIGCHLD)
            continue;

        // React to the signal
        printf("Hang-up signal received, updating configuration...\n");
        BitFieldMask mask = {0, 0, 0, 0};
        int change = ReadConfigFile(&mask);
        if (change == SITH_RET_ERR) {
            HandleErrorStatus("Failed to update configuration file");
            continue;
        }

        if (mask.changed_address || mask.changed_port) {
            // Get fresh values
            GetOptionString('a', 1, newAddr);
            GetOptionUShort('p', 1, &newPort);

            // Update address buffer
            switch (inet_pton(AF_INET, newAddr, &(addressBuffer.sin_addr.s_addr))) {
                case 1:
                    break;
                case 0:
                    printf("Malformed address received: %s\n", newAddr);
                default:
                    HandleErrorStatus("Address conversion failed");

                    SetOptionString('a', 0, address);
                    SetOptionUShort('p', 0, port);
                    goto after_address;
            }
            addressBuffer.sin_port = htons(port);

            memcpy(address, newAddr, SITH_MAXCH_IPV4 + 1);
            port = newPort;

            // Trigger handler by raising SIGUSR1
            printf("Changing server address to %s:%hu\n", address, port);
            SignalThread(listenerThread, SIGUSR1);
        }
after_address:

        if (mask.changed_max_clients) {
            // Block signals - we are creating new threads
            pthread_sigmask(SIG_SETMASK, &muteMask, &originalMask);

            unsigned int opt = 0;
            if (GetOptionUInt('u', 1, &opt)) {
                HandleErrorStatus("Failed to read client size option");
            }
            else {
                printf("Changing client count from %u to %u\n", GetThreadPoolSize(clients), opt);

                if (ResizeThreadPool(clients, opt)) {
                    if (errno == EAGAIN) {
                        // Means too many kernels are busy
                        fprintf(stderr, "Can't resize connection pool, try again\n");
                        errno = 0;
                    }
                    else {
                        HandleErrorStatus("Critical error on client pool resizing");
                    }

                    if (SetOptionUInt(SITH_SERVOPT_CLIENTS, 1, GetThreadPoolSize(clients))) {
                        HandleErrorStatus("FATAL: Can't restore configuration");
                        return EXIT_FAILURE;
                    }
                }

            }
            // Restore signals
            pthread_sigmask(SIG_SETMASK, &originalMask, NULL);
            printf("Signals restored\n");
        }

        if (mask.changed_root_dir) {

            // Change root directory
            GetOptionString('c', 1, root);
            int result = SetWorkingDirectory(root);
            if (result) {
                HandleErrorStatus("Could not set working directory");
                char* oldcwd = calloc(SITH_MAXCH_PATHNAME + SITH_MAXCH_VOPT + 1, sizeof (char));
                int res = GetWorkingDirectory(oldcwd + SITH_MAXCH_VOPT, SITH_MAXCH_PATHNAME);
                if (res) {
                    HandleErrorStatus("Could not get current working directory");
                }
                SetOptionString('c', 1, oldcwd); //TDNX check dimensions
            }
            else
                printf("Root directory changed: %s\n", root);
        }

        fflush(stdout);
    }
#endif

    // We should NEVER get here
    HandleErrorStatus("FATAL: main thread broke out the loop");
    return EXIT_FAILURE;
}
