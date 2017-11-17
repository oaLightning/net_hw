
#include <stdio.h>
#include <unistd.h>

#include "io_utils.h"
#include "string.h"
#include "defines.h"


error_code send_all(int socket, byte* data, unsigned short data_length) {
	/* Adapted from sendall function in beej's guide */
    int total = 0;        /* how many bytes we've sent 	   */
    int bytesleft = data_length; /* how many we have left to send */
    int current_send;

    while(total < data_length) {
        current_send = send(socket, data+total, bytesleft, 0);
        if (-1 == current_send) { 
        	break; 
        }
        total += current_send;
        bytesleft -= current_send;
    }

    if (total < data_length) {
    	return DIDNT_SEND_ALL_DATA;
    }
    return SUCCESS;
}

error_code send_string(int socket, char* message) {
	error_code error = SUCCESS;
	int length = strlen(message);
	error = send_all(socket, (byte*)&length, sizeof(length));
	if (SUCCESS != error) {
		return error;
	}
	return send_all(socket, (byte*)message, strlen(message));
}

error_code send_finished(int socket, error_code status) {
	return send_all(socket, (byte*)&status, sizeof(status));
}

error_code recv_all(int socket, byte* data, unsigned short data_length) {
    byte* data_ptr = data;
    int bytes_recv;

    while (data_length > 0) {
        bytes_recv = recv(socket, data_ptr, data_length, 0);
        if ((-1 == bytes_recv) || (0 == bytes_recv)) {
        	break;
        }

        data_ptr += bytes_recv;
		data_length -= bytes_recv;
    }

    if (0 == bytes_recv)  {
    	return OTHER_SIDE_DISCONNECTED;
    }
    if (0 < data_length) {
    	return FAILED_TO_RECV_ALL_DATA;
    }

    return SUCCESS;
}

error_code recv_string(int socket, char* buffer) {
	int string_length = 0;
	error_code error = recv_all(socket, (byte*)&string_length, sizeof(string_length));
	if (SUCCESS != error) {
		return error;
	}
	return recv_all(socket, (byte*)buffer, string_length);
}

error_code recv_error_code(int socket, error_code* status) {
	return recv_all(socket, (byte*)status, sizeof(*status));
}

error_code read_file(char* path, byte* data, unsigned short* data_length) {
	error_code error = SUCCESS;
	FILE* fp = NULL;
	size_t result = 0;

	fp = fopen(path, "r");
	VERIFY_CONDITION(NULL != fp, error, FAIL_OPEN_READ_FILE, "Failed to open file for reading\n");

	fseek(fp , 0 , SEEK_END);
  	*data_length = ftell(fp);
  	rewind(fp);

  	VERIFY_CONDITION(MAX_FILE_SIZE >= *data_length, error, FILE_TOO_BIG, "The file to read is too big\n");

	result = fread(data, sizeof(byte), *data_length, fp);
	VERIFY_CONDITION(result == *data_length, error, FAILED_TO_READ_FILE, "Failed to read all the file's data\n");

cleanup:
	return error;
}

error_code write_file(char* path, byte* data, unsigned short data_length) {
	error_code error = SUCCESS;
	FILE* fp = NULL;
	size_t result = 0;

	fp = fopen(path, "w");
	VERIFY_CONDITION(NULL != fp, error, FAIL_OPEN_WRITE_FILE, "Failed to open file for writing\n");

	result = fwrite(data, sizeof(byte), data_length, fp);
	VERIFY_CONDITION(result == data_length, error, FAILED_TO_WRITE_FILE, "Failed to write all the file's data\n");

cleanup:
	return error;
}

void quit(int socket) {
	close(socket);
}