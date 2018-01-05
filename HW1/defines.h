#ifndef __DEFINES__
#define __DEFINES__

/* Explicitly allowed defines */
#define MAX_FILES_PER_CLIENT 15
#define MAX_CLIENTS 15
#define MAX_USER_LENGTH 25
#define MAX_FILE_SIZE 512
#define MAX_MSG_LENGTH 100

/* Implicitly allowed defines */
#define DELIMITER '\t'
#define MAX_LINE_LENGTH (MAX_USER_LENGTH * 2 + sizeof('\t') + sizeof('\n'))
#define MAX_USERS_LIST_LENGTH ((MAX_USER_LENGTH + 1) * MAX_CLIENTS)
#define MSG_FORMAT ("New message received from %s: %s\n")

// Not using the line below because it's calculated at runtime and thus when used for array
// sizes maked them dynamic arrays. Since it's bigger than 512, we lower it to 512
// #define MAX_FORMATTED_MSG_LENGTH (MAX_MSG_LENGTH + strlen(MSG_FORMAT) - 2 * strlen("%s"))
#define MAX_FORMATTED_MSG_LENGTH (129)

/* Extra defines */
#define MAX_FILE_NAME 64
#define MAX_FILE_PATH 256
#define PERSONAL_GREETING_SIZE 100
#define MAX_CMD_LENGTH 256

#endif