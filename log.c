#include "file.h"
#include "string_heap.h"
#include "error.h"

int WriteLog(const char* log_path, HeapString* buffer){
    if(log_path == NULL || buffer == NULL) return SITH_RET_ERR;
    FileSize ss = SITH_FS_INIT(0);
    FileSize offset = SITH_FS_INIT(0);
    FileSize limit = SITH_FS_INIT(0);
    size_t file = HeapStringLength(buffer);
    File* l = CreateFileObject(log_path, ss, SITH_FILEMODE_RW, SITH_OPENMODE_NEWAPPEND, 0);
    if(l == NULL) return SITH_RET_ERR;
    if(LockFileObject(l, offset, limit) != 0) return SITH_RET_ERR;
    int res = WriteToFileObject(l, HeapStringGetRaw(buffer), &file);
    if(UnlockFileObject(l, offset, limit) != 0) return SITH_RET_ERR;
    CloseFileObject(l);
    return res == 0 ? SITH_RET_OK : SITH_RET_ERR;
}

