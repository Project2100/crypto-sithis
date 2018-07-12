/*
 * File:   file.h
 * Author: llde
 * Author: Project2100
 * Brief:  Regular-file handling functions for Windows & UNIX
 *
 *
 * Implementation notes:
 *
 * - File access and modes description:
 *
 *      SITH_FILEMODE_RO:       Read only access
 *      SITH_FILEMODE_WO:       Write only access
 *      SITH_FILEMODE_RW:       Read and write access
 *
 *      SITH_OPENMODE_EXIST:    If file exists open it, else fail
 *      SITH_OPENMODE_NEW:      If file exists open it, else create a new one
 *      SITH_OPENMODE_NEWONLY:  If file exists fail, else create a new one
 *      SITH_OPENMODE_TRUNCATE: If file exists truncate to zero, else create a new one
 *
 *      SITH_MAPMODE_READ:      Mapping is read only (private)
 *      SITH_MAPMODE_WRITE:     Mapping is read/write (shared)
 *
 * - Locks are advisory on UNIX, and mandaory on Windows
 *
 * - LockObject does (should...) block on invocation
 *
 * - The abstract type FileSize is defined in order to accomodate the different
 *      definitions from the OSs, utility macros have been defined to permit
 *      type conversion, string formatting, and primitive value extraction.
 *      This type is usually a 64bit signed integer value.
 *
 * - [UNIX]: IMPORTANT: We have a hard limit of 4GiB files, by using a 32 arch
 *      and not calling 64 functions
 * https://www.gnu.org/software/libc/manual/html_node/Reading-Attributes.html
 *
 * - [WINAPI] The size of a file mapping object that is backed by a named file
 *   is limited by disk space. The size of a file view is limited to the largest
 *   available contiguous block of unreserved virtual memory. This is at most
 *   2 GB minus the virtual memory already reserved by the process.
 *
 *
 * Created on 31 May 2017, 15:57
 */

#ifndef SITH_FILE_H
#define SITH_FILE_H

#ifdef __cplusplus
extern "C" {
#endif


//------------------------------------------------------------------------------
// INCLUDES

#include "plat.h"
#include "string.h"

#ifdef __unix__
#include <fcntl.h>
#include <sys/mman.h>
#endif


#ifdef _WIN32
#define SITH_ISABSOLUTEPATH(path) (strlen( ((path)) ) > 2 && !(memcmp(((path)) + 1, ":\\", 2)))
#elif defined __unix__
#define SITH_ISABSOLUTEPATH(path) (strlen( ((path)) ) > 0 && ((path))[0]=='/')
#endif


//------------------------------------------------------------------------------
// DEFINITIONS - File Sizes

typedef struct file File;

// Abstraction on file sizes
#ifdef _WIN32
typedef LARGE_INTEGER FileSize;
// TDNX See MIDL Types for macro associations
#define SITH_FORMAT_FILESIZE "lld"

#define SITH_FS_INIT(s) (LARGE_INTEGER) {.QuadPart = ((LONGLONG) s)}
#define SITH_FS_LL(s) (s).QuadPart

#elif defined __unix__
typedef off_t FileSize;
// TDNX define conditionally around the __ILP##__ macros, see UNIX doc about data sizes
#define SITH_FORMAT_FILESIZE "lld"

#define SITH_FS_INIT(s) (off_t) ((long long) s)
#define SITH_FS_LL(s) (long long) (s)

typedef size_t SIZE_T;
#endif

extern const FileSize SITH_FS_ZERO;


//------------------------------------------------------------------------------
// DEFINITIONS - File access modes

#ifdef _WIN32
typedef unsigned long FileMode;
typedef int OpenMode;
#define SITH_TEMP_APPEND_MODE   0x1000
#define SITH_FILEMODE_RO        GENERIC_READ
#define SITH_FILEMODE_WO        GENERIC_WRITE
#define SITH_FILEMODE_RW        (GENERIC_READ | GENERIC_WRITE)

#define SITH_OPENMODE_EXIST     OPEN_EXISTING
#define SITH_OPENMODE_NEW       OPEN_ALWAYS
#define SITH_OPENMODE_NEWONLY   CREATE_NEW
#define SITH_OPENMODE_TRUNCATE  CREATE_ALWAYS
#define SITH_OPENMODE_NEWAPPEND OPEN_ALWAYS | SITH_TEMP_APPEND_MODE

#define SITH_MAPMODE_READ       FILE_MAP_READ
#define SITH_MAPMODE_WRITE      FILE_MAP_WRITE

#elif defined __unix__
typedef int FileMode;
typedef int OpenMode;

#define SITH_FILEMODE_RO        O_RDONLY
#define SITH_FILEMODE_WO        O_WRONLY
#define SITH_FILEMODE_RW        O_RDWR

#define SITH_OPENMODE_EXIST     0
#define SITH_OPENMODE_NEW       O_CREAT
#define SITH_OPENMODE_NEWONLY   (O_CREAT | O_EXCL)
#define SITH_OPENMODE_TRUNCATE  (O_CREAT | O_TRUNC)
#define SITH_OPENMODE_NEWAPPEND (O_CREAT | O_APPEND)

#define SITH_MAPMODE_READ       PROT_READ
#define SITH_MAPMODE_WRITE      PROT_READ | PROT_WRITE

#endif

#define SITH_FILEFLAG_MAP 1


//------------------------------------------------------------------------------
// FUNCTIONS

/**
 * Creates a file object according to the specified mode of operation
 *
 * Implementation note: If the SITH_FILEFLAG_MAP is set, accessMode cannot be
 * SITH_FILEMODE_WO
 *
 * @param file_path The file's path
 * @param size the required size of the file if accessMode is
 *      SITH_FILEMODE_WRITE and openMode is SITH_OPENMODE_TRUNCATE,
 *      otherwise it is ignored
 * @param accessMode Exactly one of the FILEMODE macros, which specify what
 *      operations may be done over the file
 * @param openMode Exactly one of the OPENMODE macros, which specify how to
 *      behave in opening the file
 * @param flags: An OR combination of the following flags:
 * <ul>
 * <li> SITH_FILEFLAG_MAP: Map the file to memory</li>
 * </ul>
 *
 * @return the FileObject of the specified file, or NULL if an error occurred
 */
File* CreateFileObject(
        _In_ const char* file_path,
        _In_opt_ FileSize size,
        _In_ FileMode accessMode,
        _In_ OpenMode openMode,
        _In_ int flags);

/**
 * Frees all resources of this file object
 *
 * @param file the file object to close
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int CloseFileObject(
        _In_ File* file);

/**
 * Returns this file's size.
 *
 * @param file this File object
 * @param value
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int GetFileObjectSize(
        _In_ File* file,
        _Out_ FileSize* value);

/**
 * Reads at most size bytes from this file, and puts them in buffer
 *
 * @param file this File object
 * @param buffer
 * @param size
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int ReadFromFileObject(File* file, char* buffer, size_t* size);

/**
 * Appends the contents of the buffer to this file
 *
 * @param file this File object
 * @param buffer
 * @param size
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int WriteToFileObject(File* file, const char* buffer, size_t* size);

/**
 * Locks exclusively this file's segment specified by offset and length for the calling process
 *
 * @param file this File object
 * @param offset
 * @param length
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int LockFileObject(File* file, FileSize offset, FileSize length);

/**
 * Unlocks exclusively this file's segment specified by offset and length for the calling process
 *
 * @param file this File object
 * @param offset
 * @param length
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int UnlockFileObject(File* file, FileSize offset, FileSize length);

/**
 * Deletes the file specified by the given path from the filesystem
 *
 * @param file_path
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int DeleteFilePath(
        _In_ const char* file_path);

/**
 * Creates a file mapping of a portion of this file
 *
 * Implementation note: Due to differences in how the OSs handle their length
 *      argument, I saw no choice but to include them both in the signature
 *
 * @param file this File object
 * @param baseAddress The base address of the file portion to map
 * @param pageSize [UNIX] The mapping's standard page size, as specified by the underlying OS
 * @param actualSize [Windows] The actual bytes to be mapped from the file
 * @param mode One of STIH_MAPMODE_READ or SITH_MAPMODE_WRITE
 * @return A pointer to the base address of the mapped portion to memory
 */
void* AllocateMapping(
        _In_ File* file,
        _In_ FileSize baseAddress,
        _In_ size_t pageSize,
        _In_ SIZE_T actualSize,
        _In_ int mode);

/**
 * Frees resources allocated to a file mapping/view
 *
 * @param mapptr base address of the file mapping, as returned by mmap/MapViewOfFile
 * @param pageSize the view size, as required by mmap
 * @return SITH_OK if successful, SITH_ERR otherwise
 *
 * @see munmap()
 * @see UnmapViewOfFile()
 */
int FreeMapping(
        _In_ void* mapptr,
        _In_ size_t pageSize);

/**
 * Ensures this page is written to the actual file
 *
 * @param mapptr The pointer of the mapped region
 * @param pageSize The mapping's standard page size, as specified by the underlying OS
 * @param actualSize The actual bytes mapped from the file
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int SyncMapping(
        _In_ void* mapptr,
        _In_ size_t pageSize,
        _In_ SIZE_T actualSize);

/**
 * Flushes all buffers of the given file object
 *
 * @param file The file object to synchronize
 * @return SITH_OK if successful, SITH_ERR otherwise
 */
int SyncFileObject(
        _In_ File* file);

/** 
 *Return the full path name of this file/folder
 * @param the non full path to a file.
 * @return the full path name. It's heap allocated
 */
char* GetRealPath(_In_ char* path);

#ifdef __cplusplus
}
#endif

#endif /* SITH_FILE_H */
