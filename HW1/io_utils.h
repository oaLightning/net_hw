#ifndef __NETWORK__
#define __NETWORK__


#include <sys/socket.h>
#include <arpa/inet.h>

#include "errors.h"

typedef unsigned char byte;

typedef enum {
	LIST_FILES,
	DELETE_FILE,
	ADD_FILE,
	GET_FILE,

	INVALID_COMMAND = -1,
} command_type; 

#define GREETING_MESSAGE ("Welcome! Please log in")
#define LOGIN_ERROR_MESSAGE ("User or password incorrect, terminating")

error_code send_all(int socket, byte* data, unsigned short data_length);

error_code send_string(int socket, char* message);

error_code send_finished(int socket, error_code status);

error_code recv_all(int socket, byte* data, unsigned short data_length);

error_code recv_string(int socket, char* buffer);

error_code recv_error_code(int socket, error_code* status);

error_code read_file(char* path, byte* data, unsigned short* data_length);

error_code write_file(char* path, byte* data, unsigned short data_length);

void quit(int socket);



#endif