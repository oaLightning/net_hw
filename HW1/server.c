
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>

#include "errors.h"
#include "io_utils.h"
#include "defines.h"

typedef enum {
	EXECUTABLE = 0,
	USERS_FILE = 1,
	DIR_PATH = 2,
	PORT = 3,

	MIN_PARAMETERS = PORT,
	ALL_PARAMETERS = 4,
} argv_parameters;

typedef struct {
	char username[MAX_USER_LENGTH];
	char password[MAX_USER_LENGTH];
} user_and_password;
// We are using the weird initialization syntax because of a gcc bug
// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=53119
user_and_password user_list[MAX_CLIENTS] = {{{0}, {0}}};

char* files_directory = NULL;
unsigned short port = 1337;
char* current_user = NULL;
int current_user_socket = NULL;

void get_user_file_pah(char* file_name, char dest_buffer[MAX_FILE_PATH], char* user_name) {
	int current_path_offset = 0;
	strcpy(dest_buffer, files_directory);
	current_path_offset += strlen(files_directory);
	dest_buffer[current_path_offset] = '/';
	current_path_offset += 1;
	strcpy(&dest_buffer[current_path_offset], user_name);
	current_path_offset += strlen(user_name);
	dest_buffer[current_path_offset] = '/';
	current_path_offset += 1;
	strcpy(&dest_buffer[current_path_offset], file_name);
}

void get_file_path(char* file_name, char dest_buffer[MAX_FILE_PATH]) {
	get_user_file_pah(file_name, dest_buffer, current_user);
}

error_code get_number_of_files(int* files) {
	DIR* dir = NULL;
	char full_file_path[MAX_FILE_PATH] = {0};
	struct dirent* entry = NULL;
	error_code error = SUCCESS;

	get_file_path("", full_file_path);
	dir = opendir(full_file_path);
	VERIFY_CONDITION(NULL != dir, error, FAILED_TO_OPEN_DIR_2, "Failed to open users dir on startup\n");

	*files = 0;
	while (NULL != (entry = readdir(dir))) {
		if (0 == strcmp(entry->d_name, ".") || 0 == strcmp(entry->d_name, "..")) {
			continue;
		}
		(*files)++;
	}

cleanup:
	return error;
}

error_code list_files() {
	char full_file_path[MAX_FILE_PATH] = {0};
	DIR* dir = NULL;
	struct dirent* entry = NULL;
	error_code error = SUCCESS;
	char files_in_dir[MAX_FILE_NAME][MAX_FILES_PER_CLIENT] = {{0}};
	int file_count = 0;
	int i = 0;

	get_file_path("", full_file_path);
	dir = opendir(full_file_path);
	if (NULL == dir) {
		error = FAILED_TO_OPEN_DIR;
		return send_finished(current_user_socket, error);
	} 
	while (NULL != (entry = readdir(dir))) {
		VERIFY_CONDITION(file_count < MAX_FILES_PER_CLIENT, error, TOO_MANY_FILES, "Found more files than expected in dir\n");
		if (0 == strcmp(entry->d_name, ".") || 0 == strcmp(entry->d_name, "..")) {
			continue;
		}

		strcpy(files_in_dir[file_count], entry->d_name);
		file_count++;
	}
	error = send_all(current_user_socket, (byte*)&file_count, sizeof(file_count));
	VERIFY_SUCCESS(error)
	for (i = 0; i < file_count; i++) {
		error = send_string(current_user_socket, files_in_dir[i]);
		VERIFY_SUCCESS(error)
	}

cleanup:
	closedir(dir);
	return error;
}

error_code delete_file(char* file_name) {
	char full_file_path[MAX_FILE_PATH] = {0};
	error_code error = SUCCESS;
	int unlink_result = 0;

	get_file_path(file_name, full_file_path);
	unlink_result = unlink(full_file_path);
	if (0 != unlink_result) {
		if (ENOENT == errno) {
			error = FILE_DOESNT_EXIST;
		} else {
			error = FAILED_TO_DELETE_FILE;	
		}
	}

	return send_finished(current_user_socket, error);
}

error_code add_file(char* file_name, byte* data, unsigned short data_length) {
	char full_file_path[MAX_FILE_PATH] = {0};
	error_code write_error = SUCCESS;

	get_file_path(file_name, full_file_path);
	write_error = write_file(full_file_path, data, data_length);

	return send_finished(current_user_socket, write_error);;
}

error_code get_file(char* file_name) {
	byte file_data[MAX_FILE_SIZE] = {0};
	char full_file_path[MAX_FILE_PATH] = {0};
	unsigned short data_length = 0;
	int data_length_int = 0;
	error_code read_error = SUCCESS;
	error_code send_error = SUCCESS;

	get_file_path(file_name, full_file_path);
	read_error = read_file(full_file_path, &file_data[0], &data_length);

	send_error = send_finished(current_user_socket, read_error);
	if (SUCCESS != send_error) {
		return send_error;	
	}
	if (SUCCESS != read_error) {
		return SUCCESS;
	}

	data_length_int = data_length;
	send_error = send_all(current_user_socket, (byte*)&data_length_int, sizeof(data_length_int));
	if (SUCCESS != send_error) {
		return send_error;
	}

	return send_all(current_user_socket, (byte*)&file_data, data_length);
}

error_code make_user_directory(char* user_name) {
	char full_file_path[MAX_FILE_PATH] = {0};
	int result = 0;
	error_code error = SUCCESS;

	get_user_file_pah("", full_file_path, user_name);
	result = mkdir(full_file_path, 0777);
	if (0 != result) {
		VERIFY_CONDITION(EEXIST == errno, error, FAILED_TO_MAKE_USER_DIR, "Failed to make the user's directory\n");
	}

cleanup:
	return error;
}

error_code populate_user_list(char* users_file_path) {
    FILE* fp = NULL;
    size_t len = 0;
    error_code errors = SUCCESS;
    char* line = NULL;
    ssize_t read;
    int i = 0;
    int j = 0;

    fp = fopen(users_file_path, "r");
    VERIFY_CONDITION(NULL != fp, errors, USER_FILE_FAILURE, "Failed to open the users file\n");
    int current_user = 0;

    while ((read = getline(&line, &len, fp)) != -1) {
    	VERIFY_CONDITION(current_user < MAX_CLIENTS, errors, TOO_MANY_USERS, "Found too many users in the file\n");

    	for (i = 0; i < len; i++) {
    		if (line[i] == DELIMITER) {
    			line[i] = '\0';
    			for (j = i+1; j < len; j++) {
    				if (line[j] == '\r' || line[j] == '\n') {
    					line[j] = '\0';
    				}
    			}
    			strcpy(user_list[current_user].username, line);
    			strcpy(user_list[current_user].password, &line[i+1]);
    			errors = make_user_directory(user_list[current_user].username);
    			VERIFY_SUCCESS(errors);
    			break;
    		}
    	}
       	
        current_user++;
        free(line);
        line = NULL;
        len = 0;
    }

    VERIFY_CONDITION(current_user >= 1, errors, NOT_ENOUGH_USERS, "Didnt find any users in the file\n");

cleanup:
	if (NULL != line) {
		free(line);
	}
	fclose(fp);
	return errors;
}

error_code authenticate_user(char* username, char* password) {
	error_code error = AUTHENTICATION_ERROR;
	int i = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (0 == strcmp(username, user_list[i].username)) {
			if (0 == strcmp(password, user_list[i].password)) {
				error = SUCCESS;
				break;
			}
		}
	}

	return error;
}

error_code receive_and_execute() {
	char file_name[MAX_FILE_PATH] = {0};
	byte file_data[MAX_FILE_SIZE] = {0};
	int data_length = 0;
	command_type command = INVALID_COMMAND;
	error_code error = SUCCESS;

	error = recv_all(current_user_socket, (byte*)&command, sizeof(command));
	VERIFY_SUCCESS(error);
	printf("Received command\n");

	switch(command) {
		case LIST_FILES: {
			error = list_files();
			break;
		}
		case DELETE_FILE: {
			error = recv_string(current_user_socket, file_name);
			VERIFY_SUCCESS(error);
			error = delete_file(file_name);
			break;
		}
		case ADD_FILE: {
			error = recv_string(current_user_socket, file_name);
			VERIFY_SUCCESS(error);
			error = recv_all(current_user_socket, (byte*)&data_length, sizeof(data_length));
			VERIFY_SUCCESS(error);
			error = recv_all(current_user_socket, (byte*)&file_data, data_length);
			VERIFY_SUCCESS(error);
			error = add_file(file_name, file_data, data_length);
			break;
		}
		case GET_FILE: {
			error = recv_string(current_user_socket, file_name);
			VERIFY_SUCCESS(error);
			error = get_file(file_name);
			break;
		}
		default: {
			error = INVALID_COMMAND_ERROR;
			break;
		}
	}
	
cleanup:
	return error;
}

void handle_client(int fd) {
	error_code authenticate_error = SUCCESS;
	error_code errors = SUCCESS;
	char username[MAX_USER_LENGTH] = {0};
	char password[MAX_USER_LENGTH] = {0};
	char personal_greeting[PERSONAL_GREETING_SIZE] = {0};
	int user_files = 0;

	errors = send_string(fd, GREETING_MESSAGE);
	VERIFY_SUCCESS(errors);

	errors = recv_string(fd, username);
	VERIFY_SUCCESS(errors);

	errors = recv_string(fd, password);
	VERIFY_SUCCESS(errors);

	authenticate_error = authenticate_user(username, password);
	errors = send_finished(fd, authenticate_error);
	VERIFY_SUCCESS(errors);

	if (SUCCESS != authenticate_error) {
		errors = send_string(fd, LOGIN_ERROR_MESSAGE);
		VERIFY_SUCCESS(errors);

		goto cleanup;
	}

	current_user = &(username[0]);
	current_user_socket = fd;

	errors = get_number_of_files(&user_files);
	VERIFY_SUCCESS(errors);

	sprintf(personal_greeting, "Hi %s, you have %d files stored", &(username[0]), user_files);
	errors = send_string(fd, personal_greeting);
	VERIFY_SUCCESS(errors);

	while (1) {
		errors = receive_and_execute();
		VERIFY_SUCCESS(errors);
	}

cleanup:
	printf("User disconnected\n");
	current_user = NULL;
	current_user_socket = -1;
	quit(fd);
}


error_code start_listening() {
	error_code error = SUCCESS;
	struct sockaddr_in server = { 0 };
	int listen_socket = -1;
	int result = 0;
	int client_socket = -1;
	struct sockaddr_in client = { 0 };
	int sockaddr_size = sizeof(client);

	 listen_socket = socket(AF_INET , SOCK_STREAM , 0);
	 VERIFY_CONDITION(-1 != listen_socket, error, FAILED_TO_CREATE_SOCKET, "Failed to create the socket\n");
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
     
    result = bind(listen_socket, (struct sockaddr *)&server, sizeof(server));
    VERIFY_CONDITION(0 == result, error, BIND_FAILURE, "Failed to bind on the socket\n");
     
    result = listen(listen_socket , MAX_CLIENTS);
    VERIFY_CONDITION(0 == result, error, LISTEN_FAILURE, "Failed to listen on the socket\n");
     
	while (1) {
		client_socket = accept(listen_socket, (struct sockaddr *)&client, (socklen_t*)&sockaddr_size);
		if (-1 != client_socket) {
			handle_client(client_socket);
		} else {
			printf("Accepted an invalid client, moving on...\n");
		}
	}

cleanup:
    return error;
}


int main(int argc, char *argv[]) {
	char* users_file_path = NULL;
	error_code errors = SUCCESS;
	int local_port = 0;

	if (argc < MIN_PARAMETERS) {
		printf("Not enough parameters, expected at least %d but got %d\n", MIN_PARAMETERS, argc);
		return -1;
	}

	users_file_path = argv[USERS_FILE];
	files_directory = argv[DIR_PATH];
	if (argc == ALL_PARAMETERS) {
		int result = sscanf(argv[PORT], "%d", &local_port);
		if (result < 1) {
			printf("Failed to parse port parameter \"%s\"\n", argv[PORT]);
			return -1;
		}
		port = (unsigned short)local_port;
	}

	errors = populate_user_list(users_file_path);
	VERIFY_SUCCESS(errors);

	start_listening();

cleanup:
	return -1;
}