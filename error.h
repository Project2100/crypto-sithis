/*
 * File:   error.h
 * Author: Project2100
 * Author: llde
 * Brief:  Common includes for all headers
 *
 *
 * Implementation notes:
 *
 *   - errno is thread-safe since C11:
 * http://en.cppreference.com/w/c/error/errno
 *   - [WINAPI] GetLastError() is thread-safe:
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms679360(v=vs.85).aspx
 *   - [WSA] WSAGetLastError() isn't specified to be thread-safe, but is
 *     reported to be directly related to the former:
 * https://stackoverflow.com/questions/15586224/is-wsagetlasterror-just-an-alias-for-getlasterror
 * https://blogs.msdn.microsoft.com/oldnewthing/20050908-19/?p=34283/
 *
 *
 * Created on 27 September 2017, 17:14
 */

#ifndef SITH_COMMON_H
#define SITH_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif


//------------------------------------------------------------------------------
// INCLUDES

#include "plat.h"
#include <errno.h>
    

//------------------------------------------------------------------------------
// DEFINITIONS

#ifdef _WIN32
typedef struct sith_winerr {
    int en;
    DWORD winapi;
    int wsaapi;
} ErrorCode;
extern const ErrorCode SITH_E_NONE;
extern const ErrorCode SITH_E_NOTFOUND;

#define SITH_ISANERROR(code) (((code)).en!=SITH_E_NONE.en || ((code)).wsaapi!=SITH_E_NONE.wsaapi || ((code)).winapi!=SITH_E_NONE.winapi)
#define SITH_E_COMPARE(code, target) (((code)).en==((target)).en && ((code)).wsaapi==((target)).wsaapi && ((code)).winapi==((target)).winapi)

#elif defined __unix__
typedef int ErrorCode;
#define SITH_E_NONE 0
#define SITH_E_NOTFOUND ENOENT

#define SITH_ISANERROR(code) (((code)) != SITH_E_NONE)
#define SITH_E_COMPARE(code, target) (((code)) == ((target)))

#endif


//------------------------------------------------------------------------------
// FUNCTIONS

/**
 * Retrieves the current error code and prints a message composed by the
 * given string and a system message describing the error code to
 * the error stream.
 *
 * @param message
 */
void HandleErrorStatus(char* message);

/**
 * Gets the last error occurred in the calling thread
 *
 * @return
 */
ErrorCode GetErrorCode();

/**
 * Resets the error state to no error (SITH_NO_ERROR).
 */
void ClearErrors();

/**
 * Checks if the calling thread has unhandled error states
 *
 * @return nonzero value if OK, 0 otherwise
 */
int isSystemAlright();

/**
 * Sets the calling thread's error state to code
 *
 * @param code
 */
void SetErrorCode(ErrorCode code);

/**
 * Outputs the specified error code to the error stream
 *
 * Note: For the purposes of reporting OS failures, invoke HandleErrorStatus();
 *
 * @param message
 * @param code
 */
void DisplayError(char* message, ErrorCode code);


#ifdef __cplusplus
}
#endif

#endif // SITH_COMMON_H
