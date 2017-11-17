#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <errno.h>
#include <netdb.h>

#include "errors.h"
#include "io_utils.h"
#include "defines.h"

char* server_name = "localhost";
unsigned short port = 1337;
int server_connection = -1;

typedef enum {
	EXECUTABLE = 0,
	SERVER_NAME = 1,
	PORT = 2,
} argv_parameters;

int starts_with(const char *pre, const char *str)
{
    return (strncmp(pre, str, strlen(pre)) == 0);
}

char* get_next_word(char* word) {
	while (*word) {
		if (' ' == *word) {
			*word = '\0';
			return word+1;
		}
		word++;
	}
	return NULL;
}

void make_last_word(char* word) {
	while (*word) {
		if (isspace(*word)) {
			*word = '\0';
			return;
		}
		word++;
	}
}

error_code list_files() {
	error_code error = SUCCESS;
	int number_of_files = 0;
	int i = 0;
	char file_name[MAX_FILE_NAME] = {0};
	command_type command = LIST_FILES;

	error = send_all(server_connection, (byte*)&command, sizeof(command));
	VERIFY_SUCCESS(error);

	error = recv_all(server_connection, (byte*)&number_of_files, sizeof(number_of_files));
	VERIFY_SUCCESS(error);

	for (i = 0; i < number_of_files; i++) {
		error = recv_string(server_connection, &file_name[0]);
		VERIFY_SUCCESS(error);

		printf("%s\n", file_name);
		memset(file_name, 0, sizeof(file_name));
	}

cleanup:
	return error;
}

error_code delete_file(char* file_name) {
	error_code error = SUCCESS;
	error_code remote_error = SUCCESS;
	command_type command = DELETE_FILE;

	error = send_all(server_connection, (byte*)&command, sizeof(command));
	VERIFY_SUCCESS(error);

	error = send_string(server_connection, file_name);
	VERIFY_SUCCESS(error);

	error = recv_error_code(server_connection, &remote_error);
	VERIFY_SUCCESS(error);

	if (FILE_DOESNT_EXIST == remote_error) {
		printf("No such file exists!\n");
	} else if (SUCCESS == remote_error) {
		printf("File removed\n");
	} else {
		printf("Other error, %d", remote_error);
	}

cleanup:
	return error;
}

error_code add_file(char* path_to_file, char* file_name) {
	byte file_data[MAX_FILE_SIZE] = {0};
	error_code read_error = SUCCESS;
	int data_length = 0;
	error_code result = SUCCESS;
	error_code remote_result = SUCCESS;
	command_type command = ADD_FILE;

	read_error = read_file(path_to_file, &file_data[0], (unsigned short*)&data_length);
	VERIFY_SUCCESS(read_error);

	result = send_all(server_connection, (byte*)&command, sizeof(command));
	VERIFY_SUCCESS(result);

	result = send_string(server_connection, file_name);
	VERIFY_SUCCESS(result);

	result = send_all(server_connection, (byte*)&data_length, sizeof(data_length));
	VERIFY_SUCCESS(result);

	result = send_all(server_connection, (byte*)&file_data[0], data_length);
	VERIFY_SUCCESS(result);

	result = recv_error_code(server_connection, &remote_result);
	VERIFY_SUCCESS(result);

cleanup:
	if ((SUCCESS == result) && (SUCCESS == remote_result) && (SUCCESS == read_error)) {
		printf("File added\n");
	} else {
		printf("Failed to read file locally, %d\n", read_error);
	}

	return result;
}

error_code get_file(char* file_name, char* path_to_save) {
	error_code send_error = SUCCESS;
	error_code write_error = SUCCESS;
	error_code remote_read = SUCCESS;
	byte file_data[MAX_FILE_SIZE] = {0};
	int data_length = 0;
	command_type command = GET_FILE;

	send_error = send_all(server_connection, (byte*)&command, sizeof(command));
	VERIFY_SUCCESS(send_error);

	send_error = send_string(server_connection, file_name);
	VERIFY_SUCCESS(send_error);

	send_error = recv_error_code(server_connection, &remote_read);
	VERIFY_SUCCESS(send_error);

	if (SUCCESS != remote_read) {
		printf("Error reading the file on the server, %d\n", remote_read);
		return SUCCESS;
	}

	send_error = recv_all(server_connection, (byte*)&data_length, sizeof(data_length));
	VERIFY_SUCCESS(send_error);

	send_error = recv_all(server_connection, &file_data[0], data_length);
	VERIFY_SUCCESS(send_error);

	write_error = write_file(path_to_save, file_data, data_length);
	if (SUCCESS == write_error) {
		printf("File saved locally at \"%s\"\n", path_to_save);
	} else {
		printf("Error saving file locally, %d\n", write_error);
	}

cleanup:
	return send_error;
}

error_code get_and_execute_command() {
	char cmd[MAX_CMD_LENGTH] = {0};
	char* result = NULL;
	int length = 0;

	result = fgets(&cmd[0], sizeof(cmd), stdin);
	if (NULL == result) {
		return SUCCESS;
	}

	/* The last character is a new line so we delete it for simplifying all later code */
	length = strlen(&cmd[0]);
	cmd[length - 1] = '\0';

	switch (cmd[0]) {
		case 'l': {
			if (!starts_with("list_of_files", cmd)) {
				goto default_case;
			}
			return list_files();
		}
		case 'd': {
			if (!starts_with("delete_file", cmd)) {
				goto default_case;
			}
			char* file_name = get_next_word(cmd);
			make_last_word(file_name);
			return delete_file(file_name);
		}
		case 'a': {
			if (!starts_with("add_file", cmd)) {
				goto default_case;
			}
			char* path_to_file = get_next_word(cmd);
			char* new_file_name = get_next_word(path_to_file);
			make_last_word(new_file_name);
			return add_file(path_to_file, new_file_name);
		}
		case 'g': {
			if (!starts_with("get_file", cmd)) {
				goto default_case;
			}
			char* file_name = get_next_word(cmd);
			char* path_to_save = get_next_word(file_name);
			make_last_word(path_to_save);
			return get_file(file_name, path_to_save);
		}
		case 'q': {
			if (starts_with("quit", cmd)) {
				return USER_EXIT;
			}
			goto default_case;
		}
		case '\0': {
			/* This might come from the command line if we get an empty string */
			return SUCCESS;
		}
	default_case:
		default: {
			printf("Unknown command\n");
			return SUCCESS;
		}
	}
}

void talk_to_server() {
	char username[MAX_USER_LENGTH] = {0};
	char password[MAX_USER_LENGTH] = {0};
	char server_message[PERSONAL_GREETING_SIZE] = {0};
	error_code error = SUCCESS;
	error_code server_error = SUCCESS;
	int result = 0;

	error = recv_string(server_connection, &(server_message[0]));
	VERIFY_SUCCESS(error);
	printf("%s\n", server_message);
	memset(server_message, 0, sizeof(server_message));

	printf("User: ");
	result = scanf("%s", username);
	VERIFY_CONDITION(result > 0, error, FAILED_TO_READ_USERNAME, "Failed to read user name\n");

	printf("password: ");
	result = scanf("%s", password);
	VERIFY_CONDITION(result > 0, error, FAILED_TO_READ_PASSWORD, "Failed to read password\n");

	error = send_string(server_connection, username);
	VERIFY_SUCCESS(error);
	error = send_string(server_connection, password);
	VERIFY_SUCCESS(error);

	error = recv_error_code(server_connection, &server_error);
	VERIFY_SUCCESS(error);

	error = recv_string(server_connection, &(server_message[0]));
	VERIFY_SUCCESS(error);
	printf("%s\n", server_message);

	VERIFY_SUCCESS(server_error);

	while (1) {
		error = get_and_execute_command();
		VERIFY_SUCCESS(error);
	}

cleanup:
	return;
}

error_code connect_to_server() {
	int socket_fd = -1;
    error_code error = SUCCESS;
    struct hostent* host_entry;
    struct sockaddr_in their_addr;
    int result = 0;

    host_entry = gethostbyname(server_name);
    VERIFY_CONDITION(NULL != host_entry, error, CANT_GET_HOST_NAME, "Can't get the host by name\n");

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    VERIFY_CONDITION(-1 != socket_fd, error, CANT_CREATE_SOCKET, "Can't create a socket\n");

    their_addr.sin_family = AF_INET;      /* host byte order */
    their_addr.sin_port = htons(port);    /* short, network byte order */
    their_addr.sin_addr = *((struct in_addr *)host_entry->h_addr);
    bzero(&(their_addr.sin_zero), 8);     /* zero the rest of the struct */

    result = connect(socket_fd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
    VERIFY_CONDITION(0 == result, error, CANT_CONNECT_TO_SERVER, "Can't connect to the server\n");

    server_connection = socket_fd;

    talk_to_server();

cleanup:
	if (-1 != socket_fd) {
		printf("Disconnecting from server\n");
		quit(socket_fd);	
	}
	
    return error;
}

int main(int argc, char *argv[]) {
	int local_port = 0;
	error_code error = SUCCESS;

	if (argc > SERVER_NAME) {
		server_name = argv[SERVER_NAME];
	}
	if (argc > PORT) {
		int result = sscanf(argv[PORT], "%d", &local_port);
		if (result < 1) {
			printf("Failed to parse port parameter \"%s\"\n", argv[PORT]);
			return -1;
		}
		port = (unsigned short)local_port;
	}

	error = connect_to_server();
	if (SUCCESS == error) {
		return 0;
	}
	return -1;
}