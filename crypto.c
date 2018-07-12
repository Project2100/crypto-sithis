/*
 * File:   crypto.c
 * Author: Project2100
 * Author: llde
 * Brief:  Endec delegate source code
 *
 * Implementation notes on encode/decode execution:
 *
 * - [WINAPI] Zero-length files are rejected by this function;
 *   from MSDN:
 *      "An attempt to map a file with a length of 0 (zero) fails with an error
 *      code of ERROR_FILE_INVALID. Applications should test for files with a
 *      length of 0 (zero) and reject those files."
 *   ...not to mention they are pointless to encrypt.
 *
 * - [WINAPI] A mandatory lock onto the file is acquired and released during
 *   execution, though it should be redundant because of the handle
 *   share-mode set to 0.
 *
 * Created on 06 Sep 2017, 18:10
 */


#include <stdlib.h>
#include <stdio.h>

#include "error.h"
#include "file.h"

#include "pool.h"
#include "crypto.h"


//------------------------------------------------------------------------------
// UTILITY MACROS

#define SITH_ENDEC_DEFAULT_PAGE_SIZE 262144 // 256KiB
//#define SITH_ENDEC_DEFAULT_PAGE_SIZE 4194304  // 4MiB
#define SITH_ENDEC_RANDMASK_UNITS SITH_ENDEC_DEFAULT_PAGE_SIZE / sizeof(int)
#define SITH_CRYPTO_POOLNAME "crypto"
#define SITH_CRYPTO_POOLSIZE 8


//------------------------------------------------------------------------------
// PAGE ENDEC TASK

typedef SITH_TASKARG struct {
    File* sourceFile;
    File* targetFile;
    FileSize baseOffset;
    size_t pageSize;
    SIZE_T actualSize;
    int* mask;

    ErrorCode* error;
} PageInfo;

SITH_TASKBODY int XORendec(void* a) {

    PageInfo* taskParam = (PageInfo*) a;

    // Create views
    void* sourceBaseAddress = AllocateMapping(taskParam->sourceFile, taskParam->baseOffset, taskParam->pageSize, taskParam->actualSize, SITH_MAPMODE_READ);
    if (sourceBaseAddress == NULL) {
        *(taskParam->error) = GetErrorCode();
        SetErrorCode(SITH_E_NONE);
        return SITH_RET_ERR;
    }
    void* targetBaseAddress = AllocateMapping(taskParam->targetFile, taskParam->baseOffset, taskParam->pageSize, taskParam->actualSize, SITH_MAPMODE_WRITE);
    if (targetBaseAddress == NULL) {
        *(taskParam->error) = GetErrorCode();
        SetErrorCode(SITH_E_NONE);
        return SITH_RET_ERR;
    }

    // Init carets
    char* sourceCaret = (char*) sourceBaseAddress;
    char* targetCaret = (char*) targetBaseAddress;
    char* maskCaret = (char*) taskParam->mask;

    // Encrypt
    for (unsigned int i = 0; i < taskParam->actualSize; i++) {
        targetCaret[i] = sourceCaret[i]^maskCaret[i];
    }

    // Sync and free mappings
    FreeMapping(sourceBaseAddress, SITH_ENDEC_DEFAULT_PAGE_SIZE);
    SyncMapping(targetBaseAddress, SITH_ENDEC_DEFAULT_PAGE_SIZE, taskParam->actualSize);
    FreeMapping(targetBaseAddress, SITH_ENDEC_DEFAULT_PAGE_SIZE);
    free(taskParam->mask);
    free(taskParam);

    return SITH_RET_OK;
}

/*
 * Entry point for endec tasks
 *
 */
int main(int argc, char** argv) {

    // Arg check
    if (argc != 4 || argv[1] == NULL || argv[2] == NULL || argv[3] == NULL) {
        fprintf(stderr, "Arguments not provided correctly, expected 4 got %d\n", argc);
        return SITH_FAILCRYPTO_ARG;
    }

    // Parse seed
    unsigned int encrSeed;
    if (getUInteger(argv[3], &encrSeed)) {
        HandleErrorStatus("Failed reading seed");
        return SITH_FAILCRYPTO_SEED;
    }
    srand(encrSeed);

    // Open source file
    File* sourceFile = CreateFileObject(argv[1], SITH_FS_ZERO, SITH_FILEMODE_RO, SITH_OPENMODE_EXIST, SITH_FILEFLAG_MAP);
    if (sourceFile == NULL) {

        if (SITH_E_COMPARE(SITH_E_NOTFOUND, GetErrorCode())) {
            ClearErrors();
            return SITH_FAILCRYPTO_404;
        }

        if (errno == EBADF) {
            printf("File is not regular\n");
            return SITH_FAILCRYPTO_NOTREG;
        }

        // Effective only in WIN32
#ifdef _WIN32
        if (GetLastError() == ERROR_SHARING_VIOLATION) {
            printf("File is locked\n");
            return SITH_FAILCRYPTO_LOCKED;
        }
#endif

        HandleErrorStatus("Could not open source file");
        return SITH_FAILCRYPTO_FILE;
    }

    // Create target file with the right size
    FileSize size;
    if (GetFileObjectSize(sourceFile, &size)) {
        HandleErrorStatus("Could not get file size");
        return SITH_FAILCRYPTO_FILE;
    }
    File* targetFile = CreateFileObject(argv[2], size, SITH_FILEMODE_RW, SITH_OPENMODE_TRUNCATE, SITH_FILEFLAG_MAP);
    if (targetFile == NULL) {
        HandleErrorStatus("Could not create target file");
        return SITH_FAILCRYPTO_FILE;
    }

    // Lock files
    if (LockFileObject(sourceFile, SITH_FS_ZERO, size)) {
        HandleErrorStatus("Could not lock source file");
        return SITH_FAILCRYPTO_LOCKED;
    }
    if (LockFileObject(targetFile, SITH_FS_ZERO, size)) {
        HandleErrorStatus("Could not lock target file");
        return SITH_FAILCRYPTO_FILE;
    }

    // Build encryption pool
    ThreadPool* encryptPool = CreateThreadPool(SITH_CRYPTO_POOLNAME, SITH_CRYPTO_POOLSIZE);
    if (encryptPool == NULL) {
        HandleErrorStatus("Could not create task pool");
        // Bail out, there's nothing we can do
        return SITH_FAILCRYPTO_NOMEM;
    }

    // Compute number of pages needed to cover all the file, and round up if needed
    long long fileSize = SITH_FS_LL(size);
    unsigned long pageCount = (fileSize / SITH_ENDEC_DEFAULT_PAGE_SIZE);
    int remainder = (fileSize % SITH_ENDEC_DEFAULT_PAGE_SIZE);
    if (remainder != 0) pageCount++;

    // All set, print a report and start encrypting
    printf("Source: %s\nTarget: %s\nFile size: %"SITH_FORMAT_FILESIZE"\nPage size: %d\nSeed: %d\nPages: %lu\nFinal page size: %d\n\n",
            argv[1], argv[2], SITH_FS_LL(size), SITH_ENDEC_DEFAULT_PAGE_SIZE, encrSeed, pageCount, remainder);
    fflush(stdout);

    // Loop over pages
    PageInfo* info;
    ErrorCode* errors = calloc(pageCount, sizeof (ErrorCode));
    for (unsigned long pageNumber = 0; pageNumber < pageCount; pageNumber++) {

        // Build XOR mask
        int* mask = malloc(SITH_ENDEC_DEFAULT_PAGE_SIZE);
        if (mask == NULL) {
            HandleErrorStatus("Failed allocating encryption mask");
            return SITH_FAILCRYPTO_NOMEM;
        }

        // Using pointer arithmetic for performance
        int* endOfMask = mask + SITH_ENDEC_RANDMASK_UNITS;
        for (int* caret = mask; caret < endOfMask; caret++) {
            *caret = rand();
        }

        info = calloc(1, sizeof (PageInfo));
        if (info == NULL) {
            HandleErrorStatus("Failed allocating thread info");
            return SITH_FAILCRYPTO_NOMEM;
        }

        info->actualSize = (pageNumber == pageCount - 1) ? remainder : SITH_ENDEC_DEFAULT_PAGE_SIZE;
        info->baseOffset = SITH_FS_INIT(pageNumber * SITH_ENDEC_DEFAULT_PAGE_SIZE);
        info->mask = mask;
        info->pageSize = SITH_ENDEC_DEFAULT_PAGE_SIZE;
        info->sourceFile = sourceFile;
        info->targetFile = targetFile;
        info->error = errors + pageNumber;

        printf("\rProgress: %.0f%% (%lu/%lu)", (pageNumber + 1) * 100. / pageCount, pageNumber + 1, pageCount);
        fflush(stdout);

        // Spin the kernel
        if (ScheduleTask(encryptPool, XORendec, info, 1)) {
            HandleErrorStatus("Error scheduling page");
        }
    }

    // This call doubles as resource release and synchronization point
    DestroyThreadPool(encryptPool, 1);
    printf("\nEncryption finished\n");
    fflush(stdout);

    // Report all errors
    int error = 0;
    for (unsigned long index = 0; index < pageCount; index++) {
        if (SITH_ISANERROR(errors[index])) {
            error = SITH_FAILCRYPTO_ENDEC;
            fprintf(stderr, "Error while encrypting page %lu:\n", index);
            fflush(stderr);
            DisplayError("", errors[index]);
        }
    }
    fflush(stderr);
    free(errors);

    // Sync output file buffers and release all resources
    SyncFileObject(targetFile);

    if (UnlockFileObject(sourceFile, SITH_FS_ZERO, size) || UnlockFileObject(targetFile, SITH_FS_ZERO, size)) {
        HandleErrorStatus("Could not unlock source or target file");
        error = SITH_FAILCRYPTO_RELEASE;
    }
    if (CloseFileObject(sourceFile) || CloseFileObject(targetFile)) {
        HandleErrorStatus("Could not close source or target file");
        error = SITH_FAILCRYPTO_RELEASE;
    }
    if (DeleteFilePath(argv[1])) {
        HandleErrorStatus("Could not delete source file");
        error = SITH_FAILCRYPTO_RELEASE;
    }

    return error;
}
