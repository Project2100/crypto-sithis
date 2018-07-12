/*
 * File:   net.c
 * Author: Project2100
 *
 * Created on 21 July 2017, 09:44
 */

#include <stdlib.h>
#include <stdio.h>
#include "plat.h"
#include "error.h"
#include "string_heap.h"

#ifdef __unix__
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif // __unix__

#include "net.h"
#include "sync.h"

#define SITH_EOTS "\04" // End-Of-Transmission string
#define SITH_EOTC '\04' // End-Of-Transmission char

#define SITH_MESSAGE_BUFFER_LENGTH 255


//------------------------------------------------------------------------------
// PLATFORM ADJUSTMENTS
//
// - These are here to minimize preprocessor conditionals in source

#ifdef _WIN32
WSADATA data;

#ifdef __MINGW32__ //Please, no more...

#include "string.h"

// From Microsoft's ws2ipdef.h, Rev 0.4 Dec 15, 1996
#define INET_ADDRSTRLEN 22

typedef int socklen_t;

// Using InetPton and InetNtop specs from Ws2tcpip.h, Rev 0.4 Dec 15, 1996

int inet_pton(int addrFamily, const char* restrict szAddr, void* restrict sockAddr) {
    int error;

    if (szAddr == NULL || sockAddr == NULL) {
        WSASetLastError(WSAEFAULT);
        return SITH_RET_ERR;
    }

    //Copy the input string
    char* szAddress = calloc(strlen(szAddr) + 1, sizeof (char));
    memcpy(szAddress, szAddr, strlen(szAddr));

    struct in_addr* sain = (struct in_addr*) sockAddr;

    char* byte;

    switch (addrFamily) {

        case AF_INET:
            byte = strtok(szAddress, ".");
            error = getUByte(byte, &(sain->S_un.S_un_b.s_b1));
            if (error) return 0;

            byte = strtok(NULL, ".");
            error = getUByte(byte, &(sain->S_un.S_un_b.s_b2));
            if (error) return 0;

            byte = strtok(NULL, ".");
            error = getUByte(byte, &(sain->S_un.S_un_b.s_b3));
            if (error) return 0;

            byte = strtok(NULL, "\0");
            error = getUByte(byte, &(sain->S_un.S_un_b.s_b4));
            if (error) return 0;

            return 1;
        default:
            WSASetLastError(WSAEAFNOSUPPORT);
            return SITH_RET_ERR;
    }
}

const char* inet_ntop(int addrFamily, const void* restrict sockAddr, char* restrict addrBuf, socklen_t bufLen) {
    int error;

    if (addrBuf == NULL || bufLen < 16) {
        WSASetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    struct in_addr* sain = (struct in_addr*) sockAddr;

    switch (addrFamily) {
        case AF_INET:
            // MinGW lags behind even on format specifiers...
            //error = snprintf(addrBuf, bufLen, "%hhu.%hhu.%hhu.%hhu", sain->S_un.S_un_b.s_b1, sain->S_un.S_un_b.s_b2, sain->S_un.S_un_b.s_b3, sain->S_un.S_un_b.s_b4);
            error = snprintf(addrBuf, bufLen, "%u.%u.%u.%u", sain->S_un.S_un_b.s_b1, sain->S_un.S_un_b.s_b2, sain->S_un.S_un_b.s_b3, sain->S_un.S_un_b.s_b4);
            if (error < 0) {
                // We shall NEVER get here... or something has gone HORRIBLY WRONG
                exit(-1);
            }

            return addrBuf;
        default:
            WSASetLastError(WSAEAFNOSUPPORT);
            return NULL;
    }
}

// ...the horror, THE HORROR!!

#endif // __MINGW32__

#elif defined __unix__
#define closesocket(s) close(s)
#define SD_BOTH 2
#define INVALID_SOCKET -1
typedef int SOCKET;
#endif // _WIN32 - __unix__


//------------------------------------------------------------------------------
// WSA INITIALIZERS

int SocketAPIInit() {
#ifdef _WIN32
    int error = WSAStartup(2, &data);
    if (error) {
        WSASetLastError(error);
        return SITH_RET_ERR;
    }
#endif
    return SITH_RET_OK;
}

int SocketAPIDestroy() {
#ifdef _WIN32
    int error = WSACleanup();
    if (error) {
        WSASetLastError(error);
        return SITH_RET_ERR;
    }
#endif
    return SITH_RET_OK;
}


//------------------------------------------------------------------------------
// DATA STRUCTURES

struct sith_conn {
    SOCKET descriptor;
    struct sockaddr_in peerAddress;
    char receiveBuffer[SITH_MESSAGE_BUFFER_LENGTH + 1];
    LockObject* lock;
};


//------------------------------------------------------------------------------
// FUNCTIONS

int buildAddressToMemory(const char* ipAddress, unsigned short port, struct sockaddr_in* mem) {

    switch (inet_pton(AF_INET, ipAddress, &(mem->sin_addr.s_addr))) {
        case 1:
            break;
        case 0:
            errno = EINVAL;
        default:
            return SITH_RET_ERR;
    }

    mem->sin_port = htons(port);
    mem->sin_family = AF_INET;
    return SITH_RET_OK;
}

ListenerSocket* CreateServerSocket(const char* address, unsigned short port, int backlog) {
    ListenerSocket* sock = calloc(1, sizeof (ListenerSocket));
    if (sock == NULL) return NULL;

    struct sockaddr_in addr;

    // FIll address fields
    int error = buildAddressToMemory(address, port, &addr);
    if (error) {
        free(sock);
        return NULL;
    }

    // Open socket
    sock->descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->descriptor == INVALID_SOCKET) {
        free(sock);
        return NULL;
    }

    // Bind socket
    error = bind(sock->descriptor, (struct sockaddr*) &addr, sizeof (struct sockaddr_in));
    if (error) {
        // closesocket clears error state (at least in Windows), saving it
        ErrorCode c = GetErrorCode();

        // Ignoring errors on these
        closesocket(sock->descriptor);
        free(sock);

        // Restoring failure code
        SetErrorCode(c);
        return NULL;
    }

    // Start listening
    error = listen(sock->descriptor, sock->backlog = backlog);
    if (error) {
        // closesocket clears error state (at least in Windows), saving it
        ErrorCode c = GetErrorCode();

        // Ignoring errors on these
        closesocket(sock->descriptor);
        free(sock);

        // Restoring failure code
        SetErrorCode(c);
        return NULL;
    }

    return sock;
}

ConnectionSocket* AcceptFromClient(ListenerSocket* listener) {

    ConnectionSocket* sock = calloc(1, sizeof (ConnectionSocket));
    if (sock == NULL) return NULL;

    socklen_t size = sizeof (struct sockaddr_in);
    sock->descriptor = accept(listener->descriptor, (struct sockaddr*) &(sock->peerAddress), &(size));

    sock->lock = CreateLockObject();

    if (sock->descriptor == INVALID_SOCKET) {
        free(sock);
        return NULL;
    }

    memset(sock->receiveBuffer, 0, SITH_MESSAGE_BUFFER_LENGTH + 1);

    return sock;
}

ConnectionSocket* ConnectToServer(char* address, unsigned short port) {

    ConnectionSocket* sock = calloc(1, sizeof (ConnectionSocket));
    if (sock == NULL) return NULL;

    // Fill address fields
    int error = buildAddressToMemory(address, port, &(sock->peerAddress));
    if (error) {
        free(sock);
        return NULL;
    }

    // Open socket
    sock->descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (sock->descriptor == INVALID_SOCKET) {
        free(sock);
        return NULL;
    }

    // Connect to given address
    error = connect(sock->descriptor, (struct sockaddr*) &(sock->peerAddress), sizeof (struct sockaddr_in));
    if (error) {
        // closesocket clears error state (at least in Windows), saving it
        ErrorCode c = GetErrorCode();

        // Ignoring errors on these
        closesocket(sock->descriptor);
        free(sock);

        // Restoring failure code
        SetErrorCode(c);
        return NULL;
    }

    sock->lock = CreateLockObject();
    memset(sock->receiveBuffer, 0, SITH_MESSAGE_BUFFER_LENGTH + 1);

    return sock;
}

int SendToPeer(ConnectionSocket* sock, const char* message) {
    if (sock == NULL || message == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    // Check if message contains EOT chars
    for (const char* c = message; *c != '\0'; c++) {
        if (*c == SITH_EOTC) {
            errno = EBADMSG;
            return SITH_RET_ERR;
        }
    }

    DoLockObject(sock->lock);
    const char* caret = message;
    size_t remainderSize = strlen(message);
    while (remainderSize > 0) {

        //printf("Sending %s\n", caret);
        int bytes = send(sock->descriptor, caret, (remainderSize < SITH_MESSAGE_BUFFER_LENGTH) ? remainderSize : SITH_MESSAGE_BUFFER_LENGTH, 0);
        if (bytes < 0) {
            DoUnlockObject(sock->lock);
            return SITH_RET_ERR;
        }
        //printf("Sent %d bytes\n", bytes);
        caret += bytes;
        remainderSize -= bytes;
    }

    // Send EOT
    while (1) {
        switch (send(sock->descriptor, SITH_EOTS, 1, 0)) {
            case 1:
                DoUnlockObject(sock->lock);
                return SITH_RET_OK;
            case 0:
                continue;
            case -1:
            default:
                DoUnlockObject(sock->lock);
                return SITH_RET_ERR;
        }
    }

}

int ReceiveFromPeer(ConnectionSocket* sock, char** message) {
    if (sock == NULL || message == NULL) {
        errno = EINVAL;
        return SITH_RET_ERR;
    }

    DoLockObject(sock->lock);

    HeapString* buffer = CreateHeapString(sock->receiveBuffer);

    // Before looping, get any previously received bytes
    HeapString* prev = HeapStringSplitAtCharFirst(buffer, SITH_EOTC);
    // If we enconter an EOT here, we actually have a full message, return it
    if (prev != NULL) {
        //printf("Message in internal buffer: %s, %s\n", HeapStringGetRaw(prev), HeapStringGetRaw(buffer));
        // Copy to a new array
        const char* trueout = HeapStringGetRaw(prev);
        int len = HeapStringLength(prev);
        *message = calloc(len + 1, sizeof (char));
        memcpy(*message, trueout, len);

        // Update the socket's internal buffer
        memset(sock->receiveBuffer, 0, SITH_MESSAGE_BUFFER_LENGTH + 1);
        memcpy(sock->receiveBuffer, HeapStringGetRaw(buffer), HeapStringLength(buffer));

        //printf("extracted message: %s\ninternal buffer: %s\n", *message, sock->receiveBuffer);

        // return
        DisposeHeapString(prev);
        DisposeHeapString(buffer);
        DoUnlockObject(sock->lock);
        return len;
    }

    // Else, copy any byte from the socket's buffer and start receiving
    // NOTE: By send design, we can assume there are no \0s before an EOTC

    // AP180418: Use 'buffer' directly, it's already a copy
    //printf("Start receiving\n");

    // Start the recv loop
    int bytes;
    while (1) {
        // Wait for more bytes
        memset(sock->receiveBuffer, 0, SITH_MESSAGE_BUFFER_LENGTH + 1);
        bytes = recv(sock->descriptor, sock->receiveBuffer, SITH_MESSAGE_BUFFER_LENGTH, 0);

        if (bytes <= 0) {
            // Bad recv call
            DisposeHeapString(buffer);
            DoUnlockObject(sock->lock);
            return bytes;
        }

        // We got some bytes, look for EOTC
        HeapString* next = CreateHeapString(sock->receiveBuffer);
        HeapString* segm = HeapStringSplitAtCharFirst(next, SITH_EOTC);
        if (segm != NULL) {
            // EOT reached, append the last message's part to the buffer and save the rest inside this socket's own buffer
            HeapStringAppendSafe(buffer, segm);
            DisposeHeapString(segm);

            memset(sock->receiveBuffer, 0, SITH_MESSAGE_BUFFER_LENGTH + 1);
            memcpy(sock->receiveBuffer, HeapStringGetRaw(next), HeapStringLength(next));

            break;
        }

        // EOT not reached, copy all bytes received from recv and keep looping
        HeapStringAppendSafe(buffer, next);
        DisposeHeapString(next);
    }

    // Access the output
    const char* trueout = HeapStringGetRaw(buffer);
    //printf("Receive loop over, message is %s\n", trueout);

    // Copy it
    *message = calloc(strlen(trueout) + 1, sizeof (char));
    memcpy(*message, trueout, strlen(trueout));

    //printf("socket buffer: %s\n", sock->receiveBuffer);

    // We're done
    DisposeHeapString(buffer);
    DoUnlockObject(sock->lock);
    return strlen(*message);
}

int CloseConnection(ConnectionSocket* sock) {

    //DoLockObject(sock->lock);
    int error = shutdown(sock->descriptor, SD_BOTH);
    error = closesocket(sock->descriptor);
    //DoUnlockObject(sock->lock);

    // Overwrite possible lock errors, we're more interested in socket failures
    ErrorCode e = GetErrorCode();
    DestroyLockObject(sock->lock);
    SetErrorCode(e);

    free(sock);

    if (error) return SITH_RET_ERR;
    return SITH_RET_OK;
}

int CloseListener(ListenerSocket* sock) {
    int error = closesocket(sock->descriptor);
    free(sock);
    if (error) return SITH_RET_ERR;
    return SITH_RET_OK;
}

int GetPeerFullAddress(ConnectionSocket* sock, char** buffer) {

    *buffer = calloc(1, SITH_MAXCH_IPV4FULL + 1);
    if (*buffer == NULL) return SITH_RET_ERR;

    char ip[SITH_MAXCH_IPV4 + 1];

    if (inet_ntop(AF_INET, &(sock->peerAddress.sin_addr.s_addr), ip, SITH_MAXCH_IPV4 + 1) == NULL) {
        free(buffer);
        return SITH_RET_ERR;
    }

    int error = snprintf(*buffer, SITH_MAXCH_IPV4FULL + 1, "%s:%hu", ip, ntohs(sock->peerAddress.sin_port));
    if (error <= 0) {
        free(buffer);
        return SITH_RET_ERR;
    }

    return SITH_RET_OK;
}
