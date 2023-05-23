#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include "enums.h"

#define MAX_TASK_COUNT 10
#define MAX_PROGRAMMERS_CONNECTIONS 3
#define MAX_LOGGERS_CONNECTIONS 1

void HandleTCPProgrammerClient(int clntSocket, int port);
void HandleTCPLoggerClient(int clntSocket, int port);
int CreateTCPServerSocket(unsigned short port, int max_connections);
int AcceptTCPConnection(int servSock);

void *ThreadMain(void *arg);
void *ThreadMain2(void *arg);

void printTasksInfo();


struct ThreadArgs
{
    int clntSock;
    int port;
};

struct task tasks[MAX_TASK_COUNT];
int tasks_count, complete_count = 0;

sem_t tasks_handling;
sem_t print;
sem_t messages_handling;

sem_t programmers;
sem_t loggers;

int servSock, servSock2;
pthread_t threadID, threadID2;

char buffer[BUFFER_SIZE];

struct message messages_pull[BUFFER_SIZE];
int messages_cnt = 0;

int programmers_port, loggers_port;

void closeAll()
{
    printf("\n\nFINISH USING SIGINT\n\n");
    printTasksInfo();

    sem_destroy(&tasks_handling);
    sem_destroy(&print);
    sem_destroy(&messages_handling);
    sem_destroy(&programmers);
    sem_destroy(&loggers);

    close(servSock);
    close(servSock2);
}

void handleSigInt(int sig)
{
    if (sig != SIGINT)
    {
        return;
    }
    closeAll();
    pthread_kill(threadID, SIGKILL);
    pthread_kill(threadID2, SIGKILL);
    exit(0);
}

void initPulls()
{
    for (int i = 0; i < tasks_count; ++i)
    {
        struct task task = {.id = i, .executor_id = -1, .checker_id = -1, .status = -1};
        tasks[i] = task;
    }

    for (int i = 0; i < BUFFER_SIZE; ++i)
    {
        struct message message = {.text = "NULL"};
        messages_pull[i] = message;
    }
}

void *ThreadMain(void *threadArgs)
{
    sem_wait(&programmers);

    int clntSock;
    int port;

    pthread_detach(pthread_self());

    clntSock = ((struct ThreadArgs *)threadArgs)->clntSock;
    port = ((struct ThreadArgs *)threadArgs)->port;
    free(threadArgs);

    HandleTCPProgrammerClient(clntSock, port);

    sem_post(&programmers);

    return (NULL);
}

void *ThreadMain2(void *threadArgs)
{
    sem_wait(&loggers);

    int clntSock;
    int port;

    pthread_detach(pthread_self());

    clntSock = ((struct ThreadArgs *)threadArgs)->clntSock;
    port = ((struct ThreadArgs *)threadArgs)->port;
    free(threadArgs);

    HandleTCPLoggerClient(clntSock, port);

    sem_post(&loggers);

    return (NULL);
}

int CreateTCPServerSocket(unsigned short port, int max_connections)
{
    int sock;
    struct sockaddr_in echoServAddr;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket failed");
        exit(1);
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(sock, max_connections) < 0) {
        perror("listen() failed");
        exit(1);
    }

    return sock;
}

int AcceptTCPConnection(int servSock)
{
    int clntSock;
    struct sockaddr_in echoClntAddr;
    unsigned int clntLen;

    clntLen = sizeof(echoClntAddr);

    if ((clntSock = accept(servSock, (struct sockaddr *)&echoClntAddr, &clntLen)) < 0)
    {
        perror("[-]\tAccept error");
        exit(1);
    }
    printf("[+]\tClient connected\n");

    printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

    return clntSock;
}


void addLog(char *log)
{
    sem_wait(&messages_handling);
    printf("%s", log);
    if (messages_cnt <= BUFFER_SIZE)
    {
        struct message new_message = {log};
        messages_pull[messages_cnt++] = new_message;
    }
    else
    {
        free(log);
        perror("too many messages in pull");
        exit(1);
    }
    sem_wait(&messages_handling);
}



void printTasksInfo()
{
    sem_wait(&print);
    for (int j = 0; j < tasks_count; ++j)
    {
        char *log = malloc(sizeof(char) * BUFFER_SIZE);
        sprintf(log, "task with id = %d and status = %d, executed by programmer #%d, checked by programmer %d\n", tasks[j].id, tasks[j].status, tasks[j].executor_id, tasks[j].checker_id);
        addLog(log);
    }
    sem_post(&print);
}

void getWork(struct response *response, int programmer_id)
{
    for (int i = 0; i < tasks_count; ++i)
    {
        if (tasks[i].status == NEW)
        {
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d has found task with id = %d for executing\n", programmer_id, tasks[i].id);
            addLog(log);

            response->response_code = NEW_TASK;
            tasks[i].executor_id = programmer_id;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].status == FIX && tasks[i].executor_id == programmer_id)
        {
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d is fixing task with id = %d\n", programmer_id, tasks[i].id);
            addLog(log);

            response->response_code = NEW_TASK;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].status == EXECUTED && tasks[i].executor_id != programmer_id)
        {
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d has found task with id = %d for checking\n", programmer_id, tasks[i].id);
            addLog(log);

            response->response_code = CHECK_TASK;
            tasks[i].checker_id = programmer_id;
            tasks[i].status = CHECKING;
            response->task = tasks[i];
            return;
        }
        else if (tasks[i].executor_id == programmer_id && tasks[i].status == WRONG)
        {
            char *log = malloc(sizeof(char) * BUFFER_SIZE);
            sprintf(log, "programmer #%d need to fix his task with id = %d\n", programmer_id, tasks[i].id);
            addLog(log);

            response->response_code = FIX_TASK;
            tasks[i].status = FIX;
            response->task = tasks[i];
            return;
        }
    }

    if (complete_count == tasks_count)
    {
        response->response_code = FINISH;
    }
}


void sendCheckResult(struct task *task)
{
    tasks[task->id] = *task;
    if (task->status == RIGHT)
    {
        ++complete_count;

        char *log = malloc(sizeof(char) * BUFFER_SIZE);
        sprintf(log, "\n\n!!!!!!!!\tComplete count = %d\t!!!!!!!!!!\n\n", complete_count);
        addLog(log);
    }
}

int handleClientRequest(int clntSocket, struct request *request)
{

    struct task null_task = {-1, -1, -1, -1};
    struct response response = {-1, null_task};

    sem_wait(&tasks_handling);

    if (complete_count == tasks_count)
    {
        response.response_code = FINISH;

        char *log = malloc(sizeof(char) * BUFFER_SIZE);
        sprintf(log, "\n\nFINISH\n\n");
        addLog(log);
        printTasksInfo();
    }
    else
    {
        int programmer_id = request->programmer_id;
        struct task task = request->task;

        switch (request->request_code)
        {
        case 0:
            getWork(&response, programmer_id);
            break;
        case 1:
            task.status = EXECUTED;
            tasks[task.id] = task;
            getWork(&response, programmer_id);
            break;
        case 2:
            sendCheckResult(&task);
            getWork(&response, programmer_id);
            break;
        default:
            break;
        }
    }

    sem_post(&tasks_handling);

    send(clntSocket, &response, sizeof(response), 0);

    return response.response_code;
}


void receiveRequest(int sock, struct request *request)
{
    if (recv(sock, (struct request *)request, sizeof(*request), 0) < 0)
    {
        perror("recv() bad");
        exit(1);
    }

    char *log = malloc(sizeof(char) * BUFFER_SIZE);
    sprintf(log, "Server has received request = %d from programmer %d\n", request->request_code, request->programmer_id);
    addLog(log);
}

int m_index = 0;

void HandleTCPProgrammerClient(int clntSocket, int port)
{
    while (1)
    {
        struct request request = {-1, -1, -1};

        receiveRequest(clntSocket, &request);

        if (handleClientRequest(clntSocket, &request) == FINISH)
        {
            break;
        }
    }

    close(clntSocket);
}

void HandleTCPLoggerClient(int clntSocket, int port)
{
    bzero(buffer, BUFFER_SIZE);
    recv(clntSocket, buffer, sizeof(buffer), 0);
    printf("Client says: %s\n", buffer);

    sem_wait(&messages_handling);

    bzero(buffer, BUFFER_SIZE);
    strcpy(buffer, messages_pull[m_index++].text);
    printf("Server: %s\n", buffer);
    send(clntSocket, buffer, strlen(buffer), 0);

    sem_post(&messages_handling);

    close(clntSocket);
}

int main(int argc, char *argv[])
{
    (void)signal(SIGINT, handleSigInt);

    servSock2 = -1;
    int clntSock;
    int clntSock2 = -1;
    unsigned short echoServPort;
    int echoServPort2 = -1;
    struct ThreadArgs *threadArgs, *threadArgs2;

    sem_init(&tasks_handling, 0, 1);
    sem_init(&print, 0, 1);
    sem_init(&messages_handling, 0, 1);
    sem_init(&programmers, 0, 1);
    sem_init(&loggers, 0, 1);

    echoServPort = 7004;

    if (argc < 2)
    {
        fprintf(stderr, "Аргументы:  %s [SERVER PORT] [TASK_COUNT] [SERVER PORT 2]\n", argv[0]);
    }
    else
    {
        echoServPort = atoi(argv[1]);
    }

    tasks_count = MAX_TASK_COUNT;
    if (argc > 2)
    {
        tasks_count = atoi(argv[2]);
        tasks_count = (tasks_count > MAX_TASK_COUNT || tasks_count < 2) ? MAX_TASK_COUNT : tasks_count;
    }
    initPulls();

    struct message *message1;
    message1 = malloc(sizeof(struct message));
    message1->text = "Start";

    servSock = CreateTCPServerSocket(echoServPort, MAX_PROGRAMMERS_CONNECTIONS);

    if (argc > 3)
    {
        echoServPort2 = atoi(argv[3]);
    }

    if (echoServPort2 != -1 && echoServPort2 != echoServPort)
    {
        servSock2 = CreateTCPServerSocket(echoServPort2, MAX_LOGGERS_CONNECTIONS);
    }

    programmers_port = echoServPort;
    loggers_port = echoServPort2;

    while (complete_count < tasks_count)
    {
        clntSock = AcceptTCPConnection(servSock);

        if (servSock2 != -1)
        {
            clntSock2 = AcceptTCPConnection(servSock2);

            if ((threadArgs2 = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL) {
                perror("malloc()2 failed");
                exit(1);
            }
            threadArgs2->clntSock = clntSock2;
            threadArgs2->port = echoServPort2;

            if (pthread_create(&threadID2, NULL, ThreadMain2, (void *)threadArgs2) != 0) {
                perror("pthread_create()2 failed");
                exit(1);
            }
            printf("with thread %ld\n", (long int)threadID2);
        }

        if ((threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs))) == NULL) {
            perror("malloc() failed");
            exit(1);
        }
        threadArgs->clntSock = clntSock;
        threadArgs->port = echoServPort;

        if (pthread_create(&threadID, NULL, ThreadMain, (void *)threadArgs) != 0) {
            perror("pthread_create() failed");
            exit(1);
        }
        printf("with thread %ld\n", (long int)threadID);
    }
}