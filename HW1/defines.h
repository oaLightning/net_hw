#ifndef __DEFINES__
#define __DEFINES__

/* Explicitly allowed defines */
#define MAX_FILES_PER_CLIENT 15
#define MAX_CLIENTS 15
#define MAX_USER_LENGTH 25
#define MAX_FILE_SIZE 512

/* Implicitly allowed defines */
#define DELIMITER '\t'
#define MAX_LINE_LENGTH (MAX_USER_LENGTH * 2 + sizeof('\t') + sizeof('\n'))

/* Extra defines */
#define MAX_FILE_NAME 64
#define MAX_FILE_PATH 256
#define PERSONAL_GREETING_SIZE 100
#define MAX_CMD_LENGTH 256

#endif