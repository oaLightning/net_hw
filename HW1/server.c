
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

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
} list_files_data;
typedef struct {
    short name_length;
    char file_name[MAX_FILE_PATH];
} delete_file_data;
typedef struct {
    short name_length;
    unsigned short data_length;
    unsigned short received_data;
    char file_name[MAX_FILE_PATH];
    byte file_data[MAX_FILE_SIZE];
} add_file_data;
typedef struct {
    short name_length;
    char file_name[MAX_FILE_PATH];
} get_file_data;
typedef struct {
} online_users_data;
typedef struct {
    unsigned short name_length;
    unsigned short msg_length;
    char user_name[MAX_USER_LENGTH];
    char msg_data[MAX_MSG_LENGTH];
} send_msg_data;
typedef struct {
} read_msgs_data;
typedef struct {
    command_type command;
    union {
        list_files_data list;
        delete_file_data delete;
        add_file_data add;
        get_file_data get;
        online_users_data users;
        send_msg_data send;
        read_msgs_data read;
    } specific_data;
} command_data;

typedef struct {
    int socket;
    short username_length;
    short password_length;
    char username[MAX_USER_LENGTH];
    char password[MAX_USER_LENGTH];
    bool is_authenticated;
    command_data current_command_data;
} user_data;
user_data user_list[MAX_CLIENTS];

typedef struct {
    char username[MAX_USER_LENGTH];
    char password[MAX_USER_LENGTH];
} user_and_password;
user_and_password known_users[MAX_CLIENTS];

char* files_directory = NULL;
unsigned short port = 1337;

void zero_user_data(user_data* data) {
    memset(data, 0, sizeof(*data));
    data->socket = -1;
    data->current_command_data.command = INVALID_COMMAND;
}

void clear_user_command(int user_index) {
    memset(&user_list[user_index].current_command_data, 0, sizeof(user_list[user_index].current_command_data));
    user_list[user_index].current_command_data.command = INVALID_COMMAND;
}

void zero_all_user_data() {
    int i = 0;
    for (i = 0; i < MAX_CLIENTS; i++) {
        zero_user_data(&(user_list[i]));
    }
}

void get_user_file_path(char* file_name, char dest_buffer[MAX_FILE_PATH], char* user_name) {
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

void get_file_path(int user_index, char* file_name, char dest_buffer[MAX_FILE_PATH]) {
    get_user_file_path(file_name, dest_buffer, user_list[user_index].username);
}

error_code get_number_of_files(int user_index, int* files) {
    DIR* dir = NULL;
    char full_file_path[MAX_FILE_PATH] = {0};
    struct dirent* entry = NULL;
    error_code error = SUCCESS;

    get_file_path(user_index, "", full_file_path);
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

error_code save_msg(int user_index, char* dest_name, char* msg) {
    char file_name[MAX_FILE_PATH] = {0};
    char full_message[MAX_FORMATTED_MSG_LENGTH] = {0};

    get_user_file_path("Messages_received_offline.txt", &file_name[0], dest_name);
    snprintf(full_message, sizeof(full_message) - 1, MSG_FORMAT, user_list[user_index].username, msg);
    return write_file(file_name, (byte*)full_message, strlen(full_message), true);
}

bool is_known_user(char* dest_name) {
    int i = 0;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (0 == strcmp(dest_name, known_users[i].username)) {
            return true;
        }
    }

    return false;
}

int get_user_index(char* dest_name) {
    int i = 0;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (0 == strcmp(dest_name, user_list[i].username)) {
            return i;
        }
    }

    return -1;   
}

error_code list_files(int user_index) {
    char full_file_path[MAX_FILE_PATH] = {0};
    DIR* dir = NULL;
    struct dirent* entry = NULL;
    error_code error = SUCCESS;
    char files_in_dir[MAX_FILE_NAME][MAX_FILES_PER_CLIENT] = {{0}};
    int file_count = 0;
    int i = 0;

    get_file_path(user_index, "", full_file_path);
    dir = opendir(full_file_path);
    if (NULL == dir) {
        error = FAILED_TO_OPEN_DIR;
        return send_error_code(user_list[user_index].socket, error);
    } 
    while (NULL != (entry = readdir(dir))) {
        VERIFY_CONDITION(file_count < MAX_FILES_PER_CLIENT, error, TOO_MANY_FILES, "Found more files than expected in dir\n");
        if (0 == strcmp(entry->d_name, ".") || 0 == strcmp(entry->d_name, "..")) {
            continue;
        }

        strcpy(files_in_dir[file_count], entry->d_name);
        file_count++;
    }
    error = send_all(user_list[user_index].socket, (byte*)&file_count, sizeof(file_count));
    VERIFY_SUCCESS(error)
    for (i = 0; i < file_count; i++) {
        error = send_string(user_list[user_index].socket, files_in_dir[i]);
        VERIFY_SUCCESS(error)
    }

cleanup:
    closedir(dir);
    return error;
}

error_code delete_file(int user_index, char* file_name) {
    char full_file_path[MAX_FILE_PATH] = {0};
    error_code error = SUCCESS;
    int unlink_result = 0;

    printf("Deleting file %s for %d\n", file_name, user_index);
    get_file_path(user_index, file_name, full_file_path);
    unlink_result = unlink(full_file_path);
    if (0 != unlink_result) {
        if (ENOENT == errno) {
            error = FILE_DOESNT_EXIST;
        } else {
            error = FAILED_TO_DELETE_FILE;  
        }
    }

    return send_error_code(user_list[user_index].socket, error);
}

error_code add_file(int user_index, char* file_name, byte* data, unsigned short data_length) {
    printf("Adding file %s for %d with length %d and data %s\n", file_name, user_index, data_length, (char*)data);
    char full_file_path[MAX_FILE_PATH] = {0};
    error_code write_error = SUCCESS;

    get_file_path(user_index, file_name, full_file_path);
    write_error = write_file(full_file_path, data, data_length, false);

    return send_error_code(user_list[user_index].socket, write_error);
}

error_code get_file(int user_index, char* file_name) {
    byte file_data[MAX_FILE_SIZE] = {0};
    char full_file_path[MAX_FILE_PATH] = {0};
    unsigned short data_length = 0;
    int data_length_int = 0;
    error_code read_error = SUCCESS;
    error_code send_error = SUCCESS;

    get_file_path(user_index, file_name, full_file_path);
    read_error = read_file(full_file_path, &file_data[0], &data_length);

    send_error = send_error_code(user_list[user_index].socket, read_error);
    if (SUCCESS != send_error) {
        return send_error;  
    }
    if (SUCCESS != read_error) {
        return SUCCESS;
    }

    data_length_int = data_length;
    send_error = send_all(user_list[user_index].socket, (byte*)&data_length_int, sizeof(data_length_int));
    if (SUCCESS != send_error) {
        return send_error;
    }

    return send_all(user_list[user_index].socket, (byte*)&file_data, data_length);
}

error_code online_users(int user_index) {
    char full_list[MAX_USERS_LIST_LENGTH] = {0};
    int i = 0;
    bool is_first = true;

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (-1 != user_list[i].socket) {
            if (!is_first) {
                strcat(full_list, ",");
            } else {
                is_first = true;
            }
            strcat(full_list, user_list[i].username);
        }
    }
    return send_string(user_list[user_index].socket, full_list);
}

error_code send_msg(int user_index, char* dest_name, char* msg) {
    error_code error = SUCCESS;
    int dest_user_index = 0;

    VERIFY_CONDITION(is_known_user(dest_name), error, UNKOWN_USER_FOR_MESSAGE, "The received user is not known\n");

    dest_user_index = get_user_index(dest_name);
    if (-1 == dest_user_index) {
        return save_msg(user_index, dest_name, msg);
    }

    error = send_error_code(user_list[dest_user_index].socket, MSG_MAGIC);
    VERIFY_SUCCESS(error);

    error = send_string(user_list[dest_user_index].socket, user_list[user_index].username);
    VERIFY_SUCCESS(error);

    error = send_string(user_list[dest_user_index].socket, msg);
    VERIFY_SUCCESS(error);

cleanup:
    if (SUCCESS != error) {
        error = save_msg(user_index, dest_name, msg);
    }

    return error;
}

error_code read_msgs(int user_index) {
    error_code error = SUCCESS;
    FILE* fp = NULL;
    size_t len = 0;
    char* line = NULL;
    ssize_t read;
    char msg_file_path[MAX_FILE_PATH] = {0};
    unsigned short magic = END_MSG_MAGIC;

    get_user_file_path("Messages_received_offline.txt", msg_file_path, user_list[user_index].username);

    fp = fopen(msg_file_path, "r");
    if (NULL == fp) {
        return send_all(user_list[user_index].socket, (byte*)&magic, sizeof(magic));
    }

    while ((read = getline(&line, &len, fp)) != -1) {
        error = send_string(user_list[user_index].socket, line);
        VERIFY_SUCCESS(error);

        free(line);
        line = NULL;
        len = 0;
    }

    error = send_all(user_list[user_index].socket, (byte*)&magic, sizeof(magic));
    VERIFY_SUCCESS(error);

    fclose(fp);
    fp = NULL;
    unlink(msg_file_path);

cleanup:
    if (NULL != fp) {
        fclose(fp);
    }

    return error;
}

error_code make_user_directory(char* user_name) {
    char full_file_path[MAX_FILE_PATH] = {0};
    int result = 0;
    error_code error = SUCCESS;

    get_user_file_path("", full_file_path, user_name);
    result = mkdir(full_file_path, 0777);
    if (0 != result) {
        VERIFY_CONDITION(EEXIST == errno, error, FAILED_TO_MAKE_USER_DIR, "Failed to make the user's directory\n");
    }

cleanup:
    return error;
}

error_code populate_known_users(char* users_file_path) {
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
                strcpy(known_users[current_user].username, line);
                strcpy(known_users[current_user].password, &line[i+1]);
                errors = make_user_directory(known_users[current_user].username);
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

error_code authenticate_user(int user_index) {
    int i = 0;
    for (i = 0; i < MAX_CLIENTS; i++) {
        if (0 == strcmp(user_list[user_index].username, known_users[i].username)) {
            if (0 == strcmp(user_list[user_index].password, known_users[i].password)) {
                user_list[user_index].is_authenticated = true;
                return SUCCESS;
            }
        }
    }

    return AUTHENTICATION_ERROR;
}

error_code handle_user_authentication(int user_index) {
    error_code authenticate_error = SUCCESS;
    error_code errors = SUCCESS;
    char personal_greeting[PERSONAL_GREETING_SIZE] = {0};
    int user_files = 0;
    int fd = user_list[user_index].socket;

    authenticate_error = authenticate_user(user_index);
    errors = send_error_code(fd, authenticate_error);
    VERIFY_SUCCESS(errors);

    if (SUCCESS != authenticate_error) {
        errors = send_string(fd, LOGIN_ERROR_MESSAGE);
        VERIFY_SUCCESS(errors);

        goto cleanup;
    }

    errors = get_number_of_files(user_index, &user_files);
    VERIFY_SUCCESS(errors);

    sprintf(personal_greeting, "Hi %s, you have %d files stored", &(user_list[user_index].username[0]), user_files);
    errors = send_string(fd, personal_greeting);
    VERIFY_SUCCESS(errors);

cleanup:
    return errors;
}

void cleanup_client(int user_index) {
    close(user_list[user_index].socket);
    printf("Removing user %d\n", user_index);
    zero_user_data(&(user_list[user_index]));
}

void handle_new_client(int fd) {
    error_code errors = SUCCESS;
    int i = 0;

    errors = send_string(fd, GREETING_MESSAGE);
    VERIFY_SUCCESS(errors);

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (-1 == user_list[i].socket) {
            user_list[i].socket = fd;
            break;
        }
    }

cleanup:
    if (SUCCESS != errors) {
        close(fd);
    }
    
}

error_code receive_authentication_information(int user_index, bool* done) {
    int fd = user_list[user_index].socket;
    int result = 0;
    error_code errors = SUCCESS;
    unsigned short diff = 0;
    byte* buffer_start = NULL;

    *done = false;

    if (0 == user_list[user_index].username_length) {
        result = recv(fd, &user_list[user_index].username_length, sizeof(user_list[user_index].username_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 1\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (strlen(user_list[user_index].username) != user_list[user_index].username_length) {
        diff = user_list[user_index].username_length - strlen(user_list[user_index].username);
        buffer_start = (byte*)&user_list[user_index].username + strlen(user_list[user_index].username);
        result = recv(fd, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 2\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (0 == user_list[user_index].password_length) {
        result = recv(fd, &user_list[user_index].password_length, sizeof(user_list[user_index].password_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 3\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (strlen(user_list[user_index].password) != user_list[user_index].password_length) {
        diff = user_list[user_index].password_length - strlen(user_list[user_index].password);
        buffer_start = (byte*)&user_list[user_index].password + strlen(user_list[user_index].password);
        result = recv(fd, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 4\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
        errors = handle_user_authentication(user_index);
        VERIFY_SUCCESS(errors);
    }

    *done = true;

cleanup:
    return errors;
}

error_code get_list_files_params(int user_index) {
    error_code errors = list_files(user_index);
    clear_user_command(user_index);
    return errors;
}

error_code get_delete_file_params(int user_index) {
    int result = 0;
    error_code errors = SUCCESS;
    delete_file_data* command_data = &user_list[user_index].current_command_data.specific_data.delete;
    byte* buffer_start = NULL;
    unsigned short diff = 0;

    if (0 == command_data->name_length) {
        result = recv(user_list[user_index].socket, &command_data->name_length, sizeof(command_data->name_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 6\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (strlen(command_data->file_name) != command_data->name_length) {
        diff = command_data->name_length - strlen(command_data->file_name);
        buffer_start = (byte*)&command_data->file_name + strlen(command_data->file_name);
        result = recv(user_list[user_index].socket, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 7\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    errors = delete_file(user_index, command_data->file_name);
    clear_user_command(user_index);
cleanup:
    return errors;
}

error_code get_add_file_params(int user_index) {
    int result = 0;
    error_code errors = SUCCESS;
    add_file_data* command_data = &user_list[user_index].current_command_data.specific_data.add;
    byte* buffer_start = NULL;
    unsigned short diff = 0;

    if (0 == command_data->name_length) {
        result = recv(user_list[user_index].socket, &command_data->name_length, sizeof(command_data->name_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 10\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (strlen(command_data->file_name) != command_data->name_length) {
        diff = command_data->name_length - strlen(command_data->file_name);
        buffer_start = (byte*)&command_data->file_name + strlen(command_data->file_name);
        result = recv(user_list[user_index].socket, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 11\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }

    if (0 == command_data->data_length) {
        result = recv(user_list[user_index].socket, &command_data->data_length, sizeof(command_data->data_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 12\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (command_data->received_data != command_data->data_length) {
        diff = command_data->data_length - command_data->received_data;
        buffer_start = &(command_data->file_data[0]) + command_data->received_data;
        result = recv(user_list[user_index].socket, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 13\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
        command_data->received_data += result;
        if (command_data->received_data != command_data->data_length) {
            return errors;
        }
    }

    errors = add_file(user_index, command_data->file_name, command_data->file_data, command_data->data_length);
    clear_user_command(user_index);
cleanup:
    return errors;
}

error_code get_get_file_params(int user_index) {
    int result = 0;
    error_code errors = SUCCESS;
    get_file_data* command_data = &user_list[user_index].current_command_data.specific_data.get;
    byte* buffer_start = NULL;
    unsigned short diff = 0;

    if (0 == command_data->name_length) {
        result = recv(user_list[user_index].socket, &command_data->name_length, sizeof(command_data->name_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 8\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (strlen(command_data->file_name) != command_data->name_length) {
        diff = command_data->name_length - strlen(command_data->file_name);
        buffer_start = (byte*)&command_data->file_name + strlen(command_data->file_name);
        result = recv(user_list[user_index].socket, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 9\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    errors = get_file(user_index, command_data->file_name);
    clear_user_command(user_index);
cleanup:
    return errors;
}

error_code get_online_users_params(int user_index) {
    error_code errors = online_users(user_index);
    clear_user_command(user_index);
    return errors;
}

error_code get_send_msg_params(int user_index) {
    int result = 0;
    error_code errors = SUCCESS;
    send_msg_data* command_data = &user_list[user_index].current_command_data.specific_data.send;
    byte* buffer_start = NULL;
    unsigned short diff = 0;

    if (0 == command_data->name_length) {
        result = recv(user_list[user_index].socket, &command_data->name_length, sizeof(command_data->name_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 10\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (strlen(command_data->user_name) != command_data->name_length) {
        diff = command_data->name_length - strlen(command_data->user_name);
        buffer_start = (byte*)&command_data->user_name + strlen(command_data->user_name);
        result = recv(user_list[user_index].socket, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 11\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }

    if (0 == command_data->msg_length) {
        result = recv(user_list[user_index].socket, &command_data->msg_length, sizeof(command_data->msg_length), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 10\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }
    if (strlen(command_data->msg_data) != command_data->msg_length) {
        diff = command_data->msg_length - strlen(command_data->msg_data);
        buffer_start = (byte*)&command_data->msg_data + strlen(command_data->msg_data);
        result = recv(user_list[user_index].socket, buffer_start, diff, MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 11\n");
            return errors;
        }
        if (0 == result) {
            return CLIENT_DISCONNECTED;
        }
    }

    errors = send_msg(user_index, command_data->user_name, command_data->msg_data);
    clear_user_command(user_index);
cleanup:
    return errors;
}

error_code get_read_msgs_params(int user_index) {
    error_code errors = read_msgs(user_index);
    clear_user_command(user_index);
    return errors;
}

void read_socket_and_handle(int user_index) {
    error_code errors = SUCCESS;
    bool done_authentication = false;
    int result;

    if (!user_list[user_index].is_authenticated) {
        errors = receive_authentication_information(user_index, &done_authentication);
        VERIFY_SUCCESS(errors);
        if (!done_authentication) {
            return;
        }
    }
    if (INVALID_COMMAND == user_list[user_index].current_command_data.command) {
        result = recv(user_list[user_index].socket, &user_list[user_index].current_command_data.command, sizeof(user_list[user_index].current_command_data.command), MSG_DONTWAIT);
        if (0 > result) {
            VERIFY_CONDITION(EWOULDBLOCK == errno, errors, RECV_NO_WAIT_FAILED, "Failed receving data 5\n");
            return;
        }
        if (0 == result) {
            errors = CLIENT_DISCONNECTED;
            goto cleanup;    
        }
    }
    switch (user_list[user_index].current_command_data.command) {
        case LIST_FILES: {
            errors = get_list_files_params(user_index);
            break;
        }
        case DELETE_FILE: {
            errors = get_delete_file_params(user_index);
            break;
        }
        case ADD_FILE: {
            errors = get_add_file_params(user_index);
            break;
        }
        case GET_FILE: {
            errors = get_get_file_params(user_index);
            break;
        }
        case ONLINE_USERS: {
            errors = get_online_users_params(user_index);
            break;
        }
        case SEND_MSG: {
            errors = get_send_msg_params(user_index);
            break;
        }
        case READ_MSGS: {
            errors = get_read_msgs_params(user_index);
            break;
        }
        case INVALID_COMMAND: {
            // Nothing to do...
            break;
        }
    }

cleanup:
    if (SUCCESS != errors) {
        cleanup_client(user_index);
    }
}

error_code start_listening() {
    error_code error = SUCCESS;
    struct sockaddr_in server = { 0 };
    int listen_socket = -1;
    int result = 0;
    int client_socket = -1;
    struct sockaddr_in client = { 0 };
    int sockaddr_size = sizeof(client);
    int optval = 1;
    int i = 0;
    int max_fd = -1;
    fd_set read_fds;

    listen_socket = socket(AF_INET , SOCK_STREAM , 0);
    VERIFY_CONDITION(-1 != listen_socket, error, FAILED_TO_CREATE_SOCKET, "Failed to create the socket\n");

    result = setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));
    VERIFY_CONDITION(0 == result, error, SETOPT_FAILURE, "Failed to set option on socket\n");
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(port);
     
    result = bind(listen_socket, (struct sockaddr *)&server, sizeof(server));
    VERIFY_CONDITION(0 == result, error, BIND_FAILURE, "Failed to bind on the socket\n");
     
    result = listen(listen_socket , MAX_CLIENTS);
    VERIFY_CONDITION(0 == result, error, LISTEN_FAILURE, "Failed to listen on the socket\n");
     
    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(listen_socket, &read_fds);
        max_fd = listen_socket;
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (-1 != user_list[i].socket) {
                FD_SET(user_list[i].socket, &read_fds);
                if (user_list[i].socket > max_fd) {
                    max_fd = user_list[i].socket;
                }
            }
        }
        result = select(max_fd+1, &read_fds, 0, 0, 0);
        VERIFY_CONDITION(-1 != result, error, SELECT_ERROR, "Got an error during select\n");

        if (FD_ISSET(listen_socket, &read_fds)) {
            client_socket = accept(listen_socket, (struct sockaddr *)&client, (socklen_t*)&sockaddr_size);
            if (-1 != client_socket) {
                handle_new_client(client_socket);
            } else {
                printf("Accepted an invalid client, moving on...\n");
            }
        }

        for (i = 0; i < MAX_CLIENTS; i++) {
            if ((-1 != user_list[i].socket) && (FD_ISSET(user_list[i].socket, &read_fds))) {
                read_socket_and_handle(i);
            }
        }
    }

cleanup:
    return error;
}


int main(int argc, char *argv[]) {
    char* users_file_path = NULL;
    error_code errors = SUCCESS;
    int local_port = 0;

    zero_all_user_data();

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

    errors = populate_known_users(users_file_path);
    VERIFY_SUCCESS(errors);

    start_listening();

cleanup:
    return -1;
}