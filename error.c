#include <stdio.h>
#include <stdlib.h>
#include "error.h"

#ifdef _WIN32
const ErrorCode SITH_E_NONE = {
    .en = 0,
    .winapi = ERROR_SUCCESS,
    .wsaapi = ERROR_SUCCESS
};
const ErrorCode SITH_E_NOTFOUND = {
    .en = 0,
    .winapi = ERROR_FILE_NOT_FOUND,
    .wsaapi = ERROR_FILE_NOT_FOUND
};
#endif

void DisplayError(char* message, ErrorCode code) {

    if (!SITH_ISANERROR(code)) {
        // No error to report
        fprintf(stderr, "%s: No error to report\n", message);
        return;
    }

#ifdef _WIN32
    // For standard C calls in Win32
    // Beware: errno may be modified by strerror
    if (code.en != 0) {
        int olderrno = errno;
        char* e = strerror(code.en);
        fprintf(stderr, "[STD] %s: %s\n", message, e);
        fflush(stderr);
        errno = olderrno;
    }

    // We're in Windows, should be a WinAPI error
    if (code.winapi != 0) {
        LPSTR systemMessage = NULL;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                code.winapi,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR) & systemMessage,
                0,
                NULL);
        fprintf(stderr, "[WINAPI] %s: %s", message, systemMessage);
        fflush(stderr);
        free(systemMessage);
    }

    // Capturing WinSock-specific errors
    if (code.wsaapi != 0 && code.wsaapi != (int) code.winapi) {

        // No guarantee by spec on this appearing in GetLastError too
        LPSTR systemMessage = NULL;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                NULL,
                code.wsaapi,
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                (LPSTR) & systemMessage,
                0,
                NULL);
        fprintf(stderr, "[WSA] %s: %s", message, systemMessage);
        fflush(stderr);
        free(systemMessage);
    }

#elif defined __unix__
    char* e = strerror(code);
    fprintf(stderr, "%s: %s\n", message, e);
    fflush(stderr);
#endif
}

ErrorCode GetErrorCode() {
    ErrorCode c;
#ifdef _WIN32
    c.en = errno;
    c.winapi = GetLastError();
    c.wsaapi = WSAGetLastError();
#elif defined __unix__
    c = errno;
#endif
    return c;
}

void HandleErrorStatus(char* message) {
    ErrorCode c = GetErrorCode();
    DisplayError(message, c);
    ClearErrors();
}

int isSystemAlright() {
#ifdef _WIN32
    return errno == 0 && GetLastError() == NOERROR && WSAGetLastError() == 0;
#elif defined __unix__
    return errno == 0;
#endif
}

void ClearErrors() {
    SetErrorCode(SITH_E_NONE);
}

void SetErrorCode(ErrorCode c) {
#ifdef _WIN32
    errno = c.en;
    SetLastError(c.winapi);
    WSASetLastError(c.wsaapi);

#elif defined __unix__
    errno = c;
#endif
}
