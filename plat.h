/*
 * File:   plat.h
 * Author: Project2100
 *
 * Created on 25 September 2017, 16:56
 */

#ifndef PLAT_H
#define PLAT_H

#ifdef __cplusplus
extern "C" {
#endif

// Common function return values for all platform abstractions
// TDNX Detach these from plat-independent code
#define SITH_RET_OK 0
#define SITH_RET_ERR -1
        

#ifdef _WIN32

#if defined (_MSC_VER)
// In MSVC, WS2tcpip.h includes winsock2.h, which in turn includes Windows.h;
// can't reverse includes (in a canonical fashion) without overwriting symbols,
// preprocessor would complain
#include <WS2tcpip.h>
    
#elif defined (__WINE__)
//Here follows some workaround to avoid complain with missing definitions on winelib
#include <ws2tcpip.h>
#define snprintf _snprintf
#define ENOTSUP ENOSYS;
    
#else
// Else, assume we're using MinGW, winsock2 is included already in windows.h
#include <windows.h>
    
#endif // _MSC_VER

#elif defined (__unix__)
    
#ifndef __linux__
#pragma message "UNIX system different from Linux detected, some libraries may malfunction"
#else
#include <linux/version.h>
#endif
    
#include <unistd.h>
#include <string.h>

#else
#error Unsupported OS detected!
    
#endif // _WIN32 / __unix__

#ifndef __ATTR_SAL
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define __ATTR_SAL
#endif

#ifdef __cplusplus
}
#endif

#endif /* PLAT_H */

