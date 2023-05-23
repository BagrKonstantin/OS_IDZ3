#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void DieWithError(char *errorMessage);

enum REQUEST_CODE
{
    GET_WORK = 0,
    SEND_TASK = 1,
    SEND_CHECK = 2
};

enum RESPONSE_CODE
{
    UB = -1,
    NEW_TASK = 0,
    CHECK_TASK = 1,
    FIX_TASK = 2,
    FINISH = 3
};

enum STATUS
{
    NEW = -1,
    EXECUTING = 0,
    EXECUTED = 1,
    CHECKING = 2,
    WRONG = 3,
    RIGHT = 4,
    FIX = 5
};

struct task
{
    int id;
    int executor_id;
    int checker_id;

    int status;
};

struct request
{
    int request_code;
    int programmer_id;

    struct task task;
};

struct response
{
    int response_code;
    struct task task;
};

struct message
{
    char *text;
};

struct sendMessage
{
    struct message pull[BUFFER_SIZE];
};
