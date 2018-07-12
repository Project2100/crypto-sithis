
#include "file.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#ifdef __unix__
#include <sys/stat.h>
#if _XOPEN_UNIX == -1
#pragma message "This POSIX stdlib does not implement XSI extensions, GetRealPath function may not work correctly"
#endif
#endif

struct file {
#ifdef _WIN32
    HANDLE handle;
    HANDLE mapping;
#elif defined  __unix__
    int descriptor;
    FileMode mode;
#endif
};

#ifdef _WIN32
const FileSize SITH_FS_ZERO = {.QuadPart = 0LL};
#elif defined __unix__
const FileSize SITH_FS_ZERO = 0;
#if (!defined __linux__ || LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23))
#define MAP_POPULATE 0      // MAP_POPULATE macro is defined only from Linux 2.6.23
#endif
#endif


File* CreateFileObject(const char* file_path, FileSize size, FileMode accessMode, OpenMode openMode, int doMap) {

#ifdef _WIN32
    DWORD attr = GetFileAttributes(file_path);
    if (!(attr & FILE_ATTRIBUTE_NORMAL)
            && (attr & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_VIRTUAL))) {
        errno = EBADF;
        return NULL;
    }

#elif defined  __unix__
    struct stat metadata;
    if (lstat(file_path, &metadata)) {
        if (errno == ENOENT) {
            // We are possibly creating a new file, carry on; the second primitive will fail if anything is wrong
            errno = 0;
        }
        else {
            // Path is actually malformed, errno is already set by lstat
            return NULL;
        }
    }
    else if (!S_ISREG(metadata.st_mode)) {
        // Path is a valid location for something other than a regular file, bail out
        errno = EBADF;
        return NULL;
    }

#endif

    File* this = malloc(sizeof (File));
    if (this == NULL) return NULL;

#ifdef _WIN32
    int open_mode   = openMode & (~SITH_TEMP_APPEND_MODE);
    int append_mode = openMode & SITH_TEMP_APPEND_MODE;
    // Open handle
    this->handle = CreateFile(file_path, accessMode, 0, NULL, open_mode , FILE_ATTRIBUTE_NORMAL, NULL);
    if (this->handle == INVALID_HANDLE_VALUE) {
        free(this);
        return NULL;
    }

    // Check if it is a regular file
    DWORD type = GetFileType(this->handle);
    if (type != FILE_TYPE_DISK) {
        CloseHandle(this->handle);
        free(this);
        errno = EBADF;
        return NULL;
    }

    // Set the file size only in truncation case
    if (SITH_FS_LL(size) != 0 && open_mode == SITH_OPENMODE_TRUNCATE) {
        BOOL success = SetFilePointerEx(this->handle, size, NULL, FILE_BEGIN);
        if (!success) {
            CloseHandle(this->handle);
            free(this);
            return NULL;
        }
        success = SetEndOfFile(this->handle);
        if (!success) {
            CloseHandle(this->handle);
            free(this);
            return NULL;
        }
    }
    if(append_mode){
        BOOL success = SetFilePointerEx(this->handle, SITH_FS_INIT(0), NULL, FILE_END);
        if(!success) {
            CloseHandle(this->handle);
            free(this);
            return NULL;
        }
    }
    // Instantiate the FileMapping object, if requested
    if (doMap == SITH_FILEFLAG_MAP) {

        int permissions = ((accessMode == SITH_FILEMODE_RO) ? PAGE_READONLY : PAGE_READWRITE);

        // REMINDER: No invalid handle must be issued, otherwise we'll map the paging file!!!
        // Again, will find a guarantee for success
        this->mapping = CreateFileMapping(this->handle, NULL, permissions, 0, 0, NULL);
        if (this->mapping == NULL) {
            CloseHandle(this->handle);
            free(this);
            return NULL;
        }
    }
    else {
        this->mapping = INVALID_HANDLE_VALUE;
    }

#elif defined __unix__
    // UNIX does not have an intermediary object for file mappings
    (void) doMap;
    this->mode = accessMode;

    // We need to specify an access mask if O_CREAT is passed to open()
    if (openMode == SITH_OPENMODE_EXIST)
        this->descriptor = open(file_path, accessMode | openMode);
    else
        this->descriptor = open(file_path, accessMode | openMode, S_IRWXU);
    if (this->descriptor == -1) {
        free(this);
        return NULL;
    }

    // Set the file size only in truncation case
    if (size != 0 && openMode == SITH_OPENMODE_TRUNCATE) {

        int error = ftruncate(this->descriptor, size);
        if (error) {
            free(this);

            return NULL;
        }
    }
#endif

    return this;
}

int GetFileObjectSize(File* this, FileSize* fSize) {
#ifdef _WIN32
    BOOL success = GetFileSizeEx(this->handle, fSize);
    return (success == TRUE) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    struct stat stats;

    int error = fstat(this->descriptor, &(stats));
    if (error) return SITH_RET_ERR;

    *fSize = stats.st_size;

    return SITH_RET_OK;
#endif
}

int ReadFromFileObject(File* this, char* buffer, size_t* size) {
#ifdef _WIN32
    DWORD out = *size;
    BOOL success = ReadFile(this->handle, buffer, (DWORD) (*size), &out, NULL);
    if (success == TRUE) {
        *size = (size_t) out;
        return SITH_RET_OK;
    }

    else return SITH_RET_ERR;
#elif defined __unix__
    ssize_t out = read(this->descriptor, buffer, *size);
    if (out < 0) return SITH_RET_ERR;
    else {
        *size = (size_t) out;
        return SITH_RET_OK;
    }
#endif
}

int WriteToFileObject(File* this, const char* buffer, size_t* size) {
#ifdef _WIN32
    DWORD out = *size;
    BOOL success = WriteFile(this->handle, buffer, (DWORD) (*size), &out, NULL);
    if (success == TRUE) {
        *size = (size_t) out;
        return SITH_RET_OK;
    }

    else return SITH_RET_ERR;
#elif defined __unix__
    ssize_t out = write(this->descriptor, buffer, *size);
    if (out < 0) return SITH_RET_ERR;
    else {
        *size = (size_t) out;
        return SITH_RET_OK;
    }
#endif
}

int LockFileObject(File* this, FileSize base, FileSize limit) {
#ifdef _WIN32
    BOOL success = LockFile(this->handle, base.LowPart, base.HighPart, limit.LowPart, limit.HighPart);
    return (success == TRUE) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    struct flock l = {
        .l_type = this->mode == O_RDONLY ? F_RDLCK : F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start = base,
        .l_len = limit,
        .l_pid = 0
    };
    int error = fcntl(this->descriptor, F_SETLKW, &l);

    return error ? SITH_RET_ERR : SITH_RET_OK;
#endif
}

int UnlockFileObject(File* this, FileSize base, FileSize limit) {
#ifdef _WIN32
    BOOL success = UnlockFile(this->handle, base.LowPart, base.HighPart, limit.LowPart, limit.HighPart);
    return (success == TRUE) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    struct flock l = {
        .l_type = F_UNLCK,
        .l_whence = SEEK_SET,
        .l_start = base,
        .l_len = limit,
        .l_pid = 0
    };

    int error = fcntl(this->descriptor, F_SETLKW, &l);

    return error ? SITH_RET_ERR : SITH_RET_OK;
#endif
}

int CloseFileObject(File* this) {
#ifdef _WIN32
    BOOL success = CloseHandle(this->mapping);
    BOOL success2 = CloseHandle(this->handle);
    free(this);
    return (success == TRUE && success2 == TRUE) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    int error = close(this->descriptor);
    free(this);

    return error ? SITH_RET_ERR : SITH_RET_OK;
#endif
}

int DeleteFilePath(const char* file_path) {
#ifdef _WIN32
    BOOL success = DeleteFile(file_path);
    return (success == TRUE) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    int error = unlink(file_path);

    return error ? SITH_RET_ERR : SITH_RET_OK;
#endif
}

void* AllocateMapping(File* file, FileSize baseAddress, size_t pageSize, SIZE_T actualSize, int mode) {

    void* this;

#ifdef _WIN32
    (void) pageSize;

    if (file->mapping == INVALID_HANDLE_VALUE) {
        errno = EPERM;
        return NULL;
    }
	
    this = MapViewOfFile(file->mapping, mode, baseAddress.HighPart, baseAddress.LowPart, actualSize);
    if (this == NULL) {
        return NULL;
    }
#elif defined __unix__
    (void) actualSize;

    this = mmap(NULL, pageSize, mode, (mode == SITH_MAPMODE_READ) ? MAP_PRIVATE : (MAP_SHARED | MAP_POPULATE), file->descriptor, baseAddress);
    if (this == MAP_FAILED) {

        return NULL;
    }
#endif

    return this;
}

int FreeMapping(void* this, size_t pageSize) {
#ifdef _WIN32
    (void) pageSize;
    BOOL success = UnmapViewOfFile(this);
    return (success == TRUE) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    int error = munmap(this, pageSize);

    return error ? SITH_RET_ERR : SITH_RET_OK;
#endif
}

int SyncMapping(void* this, size_t pageSize, SIZE_T actualSize) {
#ifdef _WIN32
    (void) pageSize;
    BOOL success = FlushViewOfFile(this, actualSize);
    return (success == TRUE) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    (void) actualSize;
    int error = msync(this, pageSize, MS_SYNC);

    return error ? SITH_RET_ERR : SITH_RET_OK;
#endif
}

int SyncFileObject(File* file) {
#ifdef _WIN32
    return FlushFileBuffers(file->handle) ? SITH_RET_OK : SITH_RET_ERR;
#elif defined __unix__
    return fsync(file->descriptor) ? SITH_RET_ERR : SITH_RET_OK;
#endif
}


char* GetRealPath(char* path){
#ifdef __unix__
    return realpath(path, NULL);
#elif defined _WIN32
    return _fullpath(NULL, path, MAX_PATH);
#endif
}
