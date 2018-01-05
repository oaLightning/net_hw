#ifndef __NETWORK__
#define __NETWORK__

#include <stdbool.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include "errors.h"

typedef unsigned char byte;

typedef enum {
    LIST_FILES,
    DELETE_FILE,
    ADD_FILE,
    GET_FILE,
    ONLINE_USERS,
    SEND_MSG,
    READ_MSGS,

    INVALID_COMMAND = -1,
} command_type; 

// Note - This isn't in the error_codes enum because unlike the error codes it's only 2 bytes
#define END_MSG_MAGIC (0x4445) // "DE" -> "D end" -> "The end"

#define GREETING_MESSAGE ("Welcome! Please log in")
#define LOGIN_ERROR_MESSAGE ("User or password incorrect, terminating")

error_code send_all(int socket, byte* data, unsigned short data_length);
error_code recv_all(int socket, byte* data, unsigned short data_length);

error_code send_string(int socket, char* message);
error_code send_error_code(int socket, error_code status);

error_code recv_string(int socket, char* buffer);
error_code recv_error_code(int socket, error_code* status);

error_code read_file(char* path, byte* data, unsigned short* data_length);

error_code write_file(char* path, byte* data, unsigned short data_length, bool append);

void quit(int socket);



#endif