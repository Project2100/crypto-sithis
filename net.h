/*
 * File:   net.h
 * Author: llde
 * Author: Project2100
 * Brief: A collection of socket methods abstracting the socket APIs
 *
 *
 * Implementation notes:
 *
 * - Uses only IPV4 addresses
 *
 * - [WSA] Most WinSock functions (namely bind, connect) that return 0/-1 in UNIX do
 *      return 0/SOCKET_ERROR in Windows; now SOCKET_ERROR is currently defined
 *      as -1, and this code piece exploits this correspondence without using
 *      the WSA macro.
 *      Conversely, socket() isn't consistent in return types, thus the
 *      logic is expanded to accomodate the differences
 *
 *
 * - About closing sockets (MSDN article, but should apply in UNIX too):
 * https://msdn.microsoft.com/en-us/library/windows/desktop/ms738547(v=vs.85).aspx
 *
 * - About receiving messages from connected sockets:
 * https://stackoverflow.com/questions/2862071/how-large-should-my-recv-buffer-be-when-calling-recv-in-the-socket-library
 *
 * - Using constructors and destructors for WSA, apparently a common GCC extension (AP171215: Removed, now using shutdown hooks):
 * https://stackoverflow.com/questions/1526882/how-do-i-get-the-gcc-attribute-constructor-to-work-under-osx
 *
 *
 * Created on 21 July 2017, 09:44
 */

#ifndef SITH_NET_H
#define SITH_NET_H

#ifdef __cplusplus
extern "C" {
#endif


//------------------------------------------------------------------------------
// DEFINITIONS

typedef struct sith_listen ListenerSocket;
typedef struct sith_conn ConnectionSocket;

#ifdef __unix__
typedef int SOCKET;
#endif
struct sith_listen {
    SOCKET descriptor;
    int backlog;
};

#define SITH_MAXCH_IPV4 15                          // xxx.xxx.xxx.xxx
#define SITH_MAXCH_IPV4FULL (SITH_MAXCH_IPV4 + 6)   // xxx.xxx.xxx.xxx:ppppp


//------------------------------------------------------------------------------
// FUNCTIONS

// Startup-shutdown functions required by WSA
int SocketAPIInit();
int SocketAPIDestroy();

/**
 * Opens a new socket on this process at the given address and port, and starts
 * listening for incoming connections.
 *
 * @param address The socket's address
 * @param port The socket's port
 * @param backlog The max amount of pending requests this socket shall hold
 * @return The corresponding ListenerSocket object, or NULL on failure
 */
ListenerSocket* CreateServerSocket(
        _In_ const char* address,
        _In_ unsigned short port,
        _In_ int backlog);

/**
 * Creates a connection with a connection request from the given listening
 * socket. This call blocks until a connection request is received or an error
 * occurs, refer to accept() for details.
 *
 * @param listener The listening socket
 * @return A ConnectionSocket connected to the requestor, or NULL on failure
 */
ConnectionSocket* AcceptFromClient(
        _In_ ListenerSocket* listener);

/**
 * Attempts to connect to the host specified by the given address and port.
 *
 * @param address The target host's address
 * @param port The target host's port
 * @return A ConnectionSocket connected to the target host, or NULL on failure
 */
ConnectionSocket* ConnectToServer(
        _In_ char* address,
        _In_ unsigned short port);

/**
 * Sends a message to the host connected to the given ConnectionSocket.
 * This call blocks until the whole message is sent, or an error occurs;
 * see send() for details.
 *
 * @param sock The host to send the message to
 * @param message A string containing the message to send
 * @return A value greater than 0 if successful, or -1 on error
 */
int SendToPeer(
        _In_ ConnectionSocket* sock,
        _In_ const char* message);

/**
 * Receives a message from the host connected to the given ConnectionSocket.
 * This call blocks until a whole message is received, or an error occurs;
 * see recv() for details.
 *
 * Do note that this function allocates memory to accomodate the received
 * message, which in turn should be deallocated by the caller when not needed
 * anymore.
 *
 * @param sock The host to receive the message from
 * @param message A pointer address onto which the received message
 *          shall be referenced; the string is NUL-terminated.
 *          On failure, contents are undefined.
 * @return A value greater than 0 if successful, 0 if the connection has been
 *          closed, -1 on error
 */
int ReceiveFromPeer(
        _In_ ConnectionSocket* sock,
        _Out_ char** message);

/**
 * Closes the given connection to the corresponding host.
 *
 * @param sock The ConnectionSocket to close
 * @return 0 if successful, -1 otherwise
 */
int CloseConnection(
        _In_ ConnectionSocket* sock);

/**
 * Closes the given listening socket.
 *
 * @param sock The ListenerSocket to close
 * @return 0 if successful, -1 otherwise
 */
int CloseListener(
        _In_ ListenerSocket* sock);

/**
 * Returns the address of the given ConnectionSocket's peer as a string to the
 * given buffer
 *
 * @param sock The peer's ConnectionSocket
 * @param buffer The buffer which will contain the stringified address
 * @return 0 if successful, -1 otherwise
 */
int GetPeerFullAddress(
        _In_ ConnectionSocket* sock,
        _In_ char** buffer);


#ifdef __cplusplus
}
#endif

#endif /* SITH_NET_H */
