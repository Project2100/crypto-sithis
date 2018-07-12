#ifndef SITH_FILEWALKER_H
#define SITH_FILEWALKER_H

#ifdef __cplusplus
extern "C" {
#endif


#include "plat.h"
#include "file.h"
#include "string_heap.h"

#include <limits.h>
#ifdef _WIN32
#define SITH_NAMESEP '\\'
// This macro accounts for a NUL character, we're removing it for consistency with all other MAXCH macros
#define SITH_MAXCH_PATHNAME (MAX_PATH - 1)
#elif defined __unix__
#define SITH_NAMESEP '/'
#include <dirent.h>

#ifdef __linux__
#include <linux/limits.h>
#elif defined __APPLE__
#include <freebsd/limits.h>
#else
#define PATH_MAX 4096
#endif

#define SITH_MAXCH_PATHNAME PATH_MAX
#else
#define SITH_MAXCH_PATHNAME 4096
#endif
//PATH_MAX on linux is not reliable. MAX_PATH on windows is reliable until we don't use \\?\ extended paths

typedef enum retMean{
    return_file,
    return_dir,
    return_error
} ReturnWalk;

typedef struct filewalker SithWalker;


/*
 * This function  intialize a SithWalker.
 * @param a path (can be relative or absolute), as a NULL terminated string
 * @return a SithWalker (may be non-initialized)
 */
SithWalker* InitWalker(const char* path);

/*
 * Walk the directory represented by this SithWalker putting results into files HeapString
 *
 * @param walker: The walker
 * @param files : the heapstring with the content of the files
 * Remarks: If processing an high number of file it may exhaust the aviable memory.
 */
int WalkInDir(SithWalker* walker, HeapString* files);


/*
 * Walk the directory and it's subdirectories represented by this SithWalker putting results into files HeapString
 *
 * @param walker: The walker
 * @param files : the heapstring with the content of the files
 * Remarks: If processing an high number of file it may exhaust the aviable memory.
 */
int WalkInDirRecursive(SithWalker* walker, HeapString* files);


/*
 * Start the directory walking.
 * Cannot be called if it's already initialized.
 * @walker : a walker
 * @size: a pointer to a writable FileSize object.
 * @file: a string that will be filled with file path
 * @return walker_success if file, walker_dir if directory, walker_error if generic error.
 */
ReturnWalk WalkFirst(SithWalker* walker, FileSize* size, HeapString* file);


/*
 * Start the directory walking.
 * Must be called with an already initialized walker.
 * @walker : a walker
 * @size: a pointer to a writable FileSize object.
 * @file: a string that will be filled with file path
 * @return walker_success if file, walker_dir if directory, walker_error if generic error.
 */
ReturnWalk WalkNext(SithWalker* walker, FileSize* size, HeapString* file);

/*
 * Reset the walker to the initial state
 */
void WalkReset(SithWalker* walker);

/*
 * The walker is disposed and cannot be used again
 * The memory internally occupied is freed if any.
 */
void DisposeWalker(SithWalker* walker);


int GetWorkingDirectory(char* buffer, size_t length);
int SetWorkingDirectory(const char* path);


#ifdef __cplusplus
}
#endif

#endif /* SITH_FILEWALKER_H */
