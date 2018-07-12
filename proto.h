/* 
 * File:   proto.h
 * Author: Project2100
 * Brief:  Protocol implementation as specified by request
 *
 * Created on 08 September 2017, 16:18
 */

#ifndef SITH_PROTO_H
#define SITH_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#define SITH_MAXCH_PROTOCMD 5
#define SITH_MAXCH_PROTORESP 3


//------------------------------------------------------------------------------
// SERVER RESPONSES

#define SITH_PROTO_ACCEPTED     "100"

#define SITH_PROTO_SUCCESS      "200"

#define SITH_PROTO_MOREOUT      "300"
#define SITH_PROTO_MOREEND      "301"

#define SITH_PROTO_INVALID      "400"

#define SITH_PROTO_FAILURE      "500"
#define SITH_PROTO_SERVBUSY     "503"
#define SITH_PROTO_NOTIMPL      "542"


//------------------------------------------------------------------------------
// PROTOCOL COMMANDS

// richiede la lista dei soli file presenti nella cartella corrente (non entra in eventuali sottocartelle)
#define SITH_PROTO_LIST "LSTF\n"

// richiede la lista di tutti i file presenti nella cartella corrente e nelle sottocartelle.
#define SITH_PROTO_LISTREC "LSTR\n"

// cifra con il metodo dello XOR il file path utilizzando seed (un unsigned int) come seme del generatore random rand().
#define SITH_PROTO_ENCRYPT "ENCR "

// decifra con il metodo dello XOR il file path utilizzando seed (un unsigned int) come seme del generatore random rand().
#define SITH_PROTO_DECRYPT "DECR "


//------------------------------------------------------------------------------
// CLIENT COMMANDS

// Displays all available commands
#define SITH_CMD_HELP "help\n"

// Terminates connection and closes the client
#define SITH_CMD_EXIT "exit\n"

// Terminates connection and closes the client
#define SITH_CMD_EXIT2 "quit\n"

// Reports command queue    
#define SITH_CMD_QUEUE "queue\n"

// Reports command queue    
#define SITH_CMD_CLEAR "clear\n"

// LSTF
#define SITH_CMD_LIST "list\n"

// LSTR
#define SITH_CMD_LISTREC "listrec\n"

// ENCR
#define SITH_CMD_ENCRYPT "encrypt "

// DECR
#define SITH_CMD_DECRYPT "decrypt "


//------------------------------------------------------------------------------
// UTILITY MACROS

#define SERVER_RESPONSE(buffer, code) memcmp(((buffer)),((code)), SITH_MAXCH_PROTORESP) == 0
#define CLIENT_REQUEST(buffer, code) memcmp(((buffer)),((code)), SITH_MAXCH_PROTOCMD) == 0
#define CLIENT_COMMAND(buffer, code) compareUntilChar(((buffer)), ((code)), strlen(((code))), ((code))[strlen(((code))) - 1]) == 0


#ifdef __cplusplus
}
#endif

#endif /* SITH_PROTO_H */
