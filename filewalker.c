#include <stdio.h>
#include <stdlib.h>

#include "string.h"
#include "error.h"
#include "filewalker.h"

#ifdef __unix__
#include <sys/stat.h>
#endif

struct filewalker {
    HeapString* root;
    unsigned short main_folder;
#ifdef __unix__
    DIR* currentDir;
#elif defined _WIN32
    HANDLE searchHandle;
#endif
};


#ifdef _WIN32
typedef WIN32_FIND_DATA DirEntry;
#elif defined __unix__
typedef struct stat DirEntry;
#endif

SithWalker* InitWalker(const char* path) {
    SithWalker* walker = calloc(1, sizeof(SithWalker));
    if (path == NULL || walker == NULL) return NULL;

    HeapString* StringRoot = NULL;
#ifdef __unix__
    char* root = realpath(path, NULL);
    if (root == NULL) {
        free(walker);
        return NULL;
    }
    StringRoot = CreateHeapString(root);
    HeapStringAppend(StringRoot, "/");
    free(root);
    if (StringRoot == NULL) {
        free(walker);
        return NULL;
    }

    walker->currentDir = NULL;
#elif defined _WIN32
    // Reallocating to accomodate wildcard
    char* pat = _fullpath(NULL, path, MAX_PATH);
    StringRoot = CreateHeapString(pat);
    free(pat);
    //int pathlen = strlen(path);
    if (StringRoot == NULL){
        free(walker);
        return NULL;
    }
    HeapStringAppend(StringRoot, "\\*");
    // Win32 inits this field on first record retrieval
    walker->searchHandle = NULL;
#endif
    walker->root = StringRoot;
    walker->main_folder = 1;
/*
    printf("Init Walker: %s\n", HeapStringGetRaw(walker->root));
*/
    return walker;
}

ReturnWalk WalkFirst(SithWalker* walker, FileSize* size, HeapString* file) {
    if (file == NULL) return return_error;

#ifdef __unix__
    if (walker->currentDir != NULL) return return_error;

    if (!(walker->currentDir = opendir(HeapStringGetRaw(walker->root)))) return return_error;
    struct dirent* drent = NULL;
    if (!(drent = readdir(walker->currentDir))) return return_error;
    while (strcmp(drent->d_name, ".") == 0 || strcmp(drent->d_name, "..") == 0) {
        drent = readdir(walker->currentDir);
        if (drent == NULL) return return_error;
    }
    struct stat statt;
    HeapStringSetSafe(file, walker->root);
    HeapStringAppend(file, drent->d_name);
    if (lstat(HeapStringGetRaw(file), &statt) == -1) {
        perror("stat: ");
        printf("%s%s\n", HeapStringGetRaw(walker->root), drent->d_name);
        return return_error;
    }
    // printf("%-10ld %s\r\n", statt.st_size, HeapStringGetRaw(file));
    //   strcpy(file, drent->d_name);
    *size = statt.st_size;
    if (S_ISDIR(statt.st_mode)) return return_dir;
    else return return_file;
#elif defined _WIN32
    if (walker->searchHandle != NULL) return return_error;

    WIN32_FIND_DATA data;

    walker->searchHandle = FindFirstFile(HeapStringGetRaw(walker->root), &data);
    if (walker->searchHandle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_NO_MORE_FILES) SetLastError(NOERROR);
        return return_error;
    }
    while (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
        BOOL success = FindNextFile(walker->searchHandle, &data);
        if (success == 0) return return_error;
    }

    size->LowPart = data.nFileSizeLow;
    size->HighPart = data.nFileSizeHigh;
    HeapStringSetSafe(file, walker->root);
    HeapStringTruncate(file, 1);
    HeapStringAppend(file, data.cFileName);
    //  printf("%-10lld %s\n", size->QuadPart, HeapStringGetRaw(file));
    DWORD attrib = GetFileAttributes(HeapStringGetRaw(file));
    if (attrib == INVALID_FILE_ATTRIBUTES) return return_error;
    if ((attrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) return return_dir;
    else return return_file;

#endif
    return return_file;
}

ReturnWalk WalkNext(SithWalker* walker, FileSize* size, HeapString* file) {
    if (file == NULL) return return_error;
#ifdef __unix__
    if (walker->currentDir == NULL) return return_error;
    struct dirent* drent = NULL;
    if (!(drent = readdir(walker->currentDir))) return return_error;
    while (strcmp(drent->d_name, ".") == 0 || strcmp(drent->d_name, "..") == 0) {
        drent = readdir(walker->currentDir);
        if (drent == NULL) return return_error;
    }
    struct stat statt;
    //    sprintf(rec, "%s/%s", walker->root, drent->d_name);
    HeapStringSetSafe(file, walker->root);
    HeapStringAppend(file, drent->d_name);
    if (lstat(HeapStringGetRaw(file), &statt) == -1) {
        perror("stat: ");
        printf("%s\n", drent->d_name);
        return return_error;
    }
    //printf("%-10ld %s\r\n", statt.st_size, HeapStringGetRaw(file));
    // strcpy(file, drent->d_name);
    *size = statt.st_size;
    if (S_ISDIR(statt.st_mode)) return return_dir;
    else return return_file;
#else
    if (walker->searchHandle == NULL) return return_error;
    WIN32_FIND_DATA data;
    BOOL success = FindNextFile(walker->searchHandle, &data);
    if (success == 0) return return_error;
    while (strcmp(data.cFileName, ".") == 0 || strcmp(data.cFileName, "..") == 0) {
        success = FindNextFile(walker->searchHandle, &data);
        if (success == 0) return return_error;
    }
    size->LowPart = data.nFileSizeLow;
    size->HighPart = data.nFileSizeHigh;
    HeapStringSetSafe(file, walker->root);
    HeapStringTruncate(file, 1);
    HeapStringAppend(file, data.cFileName);
    DWORD attrib = GetFileAttributes(HeapStringGetRaw(file));
    //printf("%-10lld %s\n", size->QuadPart, HeapStringGetRaw(file));
    if (attrib == INVALID_FILE_ATTRIBUTES) return return_error;
    if ((attrib & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) return return_dir;
    else return return_file;
#endif
}

int WalkInDir(SithWalker* walker, HeapString* files) {
    if (files == NULL) return SITH_RET_ERR;
    FileSize size = SITH_FS_INIT(0);
    HeapString* string = CreateHeapString("");
    ReturnWalk st = WalkFirst(walker, &size, string);
    if (st == return_error) return SITH_RET_ERR;
    char sizeformatted[22] = {0};
    snprintf(sizeformatted, 22, "%-15"SITH_FORMAT_FILESIZE" ", SITH_FS_LL(size));
    HeapStringSet(files, sizeformatted);
    HeapStringAppendSafe(files, string);
    HeapStringAppend(files, "\r\n");
    while (1) {
        if (WalkNext(walker, &size, string) == return_error) break;
        char sizeformat[22] = {0};
        snprintf(sizeformat, 22, "%-15"SITH_FORMAT_FILESIZE" ", SITH_FS_LL(size));
        HeapStringAppend(files, sizeformat);
        HeapStringAppendSafe(files, string);
        HeapStringAppend(files, "\r\n");
    }
    HeapStringAppend(files, ".\r\n");
    /*
        printf("%s\n", HeapStringGetRaw(files));
     */
    DisposeHeapString(string);
    return SITH_RET_OK;
}

int WalkInDirRecursive(SithWalker* walker, HeapString* files) {
    if (files == NULL) return SITH_RET_ERR;
    FileSize size = SITH_FS_INIT(0);
    HeapString* string = CreateHeapString("");
    ReturnWalk st = WalkFirst(walker, &size, string);
    if(st == return_error) return SITH_RET_ERR;
    if(st == return_dir){
        SithWalker* recursed = InitWalker(HeapStringGetRaw(string));
        recursed->main_folder = 0;
        WalkInDirRecursive(recursed, files);
        DisposeWalker(recursed);
    }
    char sizeformatted[22] = {0};
    snprintf(sizeformatted, 22, "%-15"SITH_FORMAT_FILESIZE" ", SITH_FS_LL(size));
    HeapStringAppend(files, sizeformatted);
    HeapStringAppendSafe(files, string);
    HeapStringAppend(files, "\r\n");
    while (1) {
        ReturnWalk ret = WalkNext(walker, &size, string);
        if (ret == return_error) break;
        if (ret == return_dir) {
            SithWalker* recursed = InitWalker(HeapStringGetRaw(string));
            recursed->main_folder = 0;
            WalkInDirRecursive(recursed, files);
            DisposeWalker(recursed);
        }
        char sizeformat[22] = {0};
        snprintf(sizeformat, 22, "%-15"SITH_FORMAT_FILESIZE" ", SITH_FS_LL(size));
        HeapStringAppend(files, sizeformat);
        HeapStringAppendSafe(files, string);
        HeapStringAppend(files, "\r\n");
    }
    if (walker->main_folder) HeapStringAppend(files, ".\r\n");
    DisposeHeapString(string);
    return SITH_RET_OK;
}

void DisposeWalker(SithWalker* walker) {
    if (walker == NULL) return;
    DisposeHeapString(walker->root);
    walker->root = NULL;
#ifdef __unix__
    closedir(walker->currentDir);
    walker->currentDir = NULL;
#endif
    free(walker);
}


int GetWorkingDirectory(char* buffer, size_t length) {
#ifdef _WIN32
    //NOTES: Using WIN32 MAX_PATH, but the function's NULL mechanism is more reliable...
    int error = GetCurrentDirectory(length, buffer);
    if (error == 0) return SITH_RET_ERR;
#elif defined __unix__
    char* out = getcwd(buffer, length);
    if (out == NULL) return SITH_RET_ERR;
#endif
    return SITH_RET_OK;
}

int SetWorkingDirectory(const char* path) {
#ifdef _WIN32
    BOOL success = SetCurrentDirectory(path);
    if (success == FALSE) return SITH_RET_ERR;
#elif defined __unix__
    int error = chdir(path);
    if (error) return SITH_RET_ERR;
#endif
    return SITH_RET_OK;
}
/*

// WIP Error modes
FileDesType GetFileType(const char* path) {
#ifdef _WIN32
    int attrib = GetFileAttributes(path);
    if (attrib & FILE_ATTRIBUTE_DIRECTORY) return 0;
#elif defined __unix__

    struct stat st_buf;
    int status = lstat(path, &st_buf);
    if (status) {
        return fdt_invalid;
    }

    if (S_ISREG(st_buf.st_mode)) {
        return 0;
    }

    return -1;


#endif
}
*/
