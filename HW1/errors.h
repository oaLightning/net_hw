#ifndef __ERRORS__
#define __ERRORS__

typedef enum {
	SUCCESS = 0,
	FAILED_TO_CREATE_SOCKET,
	BIND_FAILURE,
	USER_FILE_FAILURE,
	NOT_ENOUGH_USERS,
	AUTHENTICATION_ERROR,
	FAILED_TO_DELETE_FILE,
	INVALID_COMMAND_ERROR,
	FAILED_TO_OPEN_DIR,
	TOO_MANY_FILES,
	FAIL_OPEN_WRITE_FILE,
	FAILED_TO_WRITE_FILE,
	DIDNT_SEND_ALL_DATA,
	FAILED_TO_RECV_ALL_DATA,
	LISTEN_FAILURE,
	FAIL_OPEN_READ_FILE,
	FILE_TOO_BIG,
	FAILED_TO_READ_FILE,
	TOO_MANY_USERS,
	FAILED_TO_MAKE_USER_DIR,
	FAILED_TO_OPEN_DIR_2,
	FAILED_TO_READ_USERNAME,
	FAILED_TO_READ_PASSWORD,
	FILE_DOESNT_EXIST,
	NO_INPUT,
	USER_EXIT,
	CANT_GET_HOST_NAME,
	CANT_CREATE_SOCKET,
	CANT_CONNECT_TO_SERVER,
	OTHER_SIDE_DISCONNECTED,
} error_code;

#define VERIFY_SUCCESS(error_code)          \
	if (SUCCESS != (error_code)) {  		\
		goto cleanup;						\
	}

#define VERIFY_CONDITION(cond, error, code, message) \
	if (!(cond)) {							 \
		printf(message);					 \
		error = code;						 \
		goto cleanup;						 \
	}


#endif