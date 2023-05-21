#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "enums.h"

#define MAX_TASK_COUNT 10
#define MAX_CLIENT_CONNECTIONS 3

void HandleTCPClient(int clntSocket);

struct ThreadArgs {
  int clntSock;
  int tasks_count;
};

struct task tasks[MAX_TASK_COUNT];

int tasks_count, complete_count = 0;

sem_t sem;
sem_t print;

void initPulls() {
    for (int i = 0; i < tasks_count; ++i) {
        struct task task = {.id = i, .executor_id = -1, .checker_id = -1, .status = -1};
        tasks[i] = task;
    }
}

void *ThreadMain(void *threadArgs) {
    int clntSock;
    int count;

    pthread_detach(pthread_self());

    clntSock = ((struct ThreadArgs *) threadArgs)->clntSock;
    count = ((struct ThreadArgs *) threadArgs)->tasks_count;
    free(threadArgs);
    HandleTCPClient(clntSock);

    return (NULL);
}

int CreateTCPServerSocket(unsigned short port) {
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

    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) {
        perror("bind failed");
        exit(1);
    }

    if (listen(sock, MAX_CLIENT_CONNECTIONS) < 0) {
        perror("listen failed");
        exit(1);
    }

    return sock;
}

int AcceptTCPConnection(int servSock) {
    int clntSock;
    struct sockaddr_in echoClntAddr;
    unsigned int clntLen;

    clntLen = sizeof(echoClntAddr);

    if ((clntSock = accept(servSock, (struct sockaddr *) &echoClntAddr, &clntLen)) < 0) {
        perror("Accept error");
        exit(1);
    }
    printf("Client connected\n");

    printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

    return clntSock;
}

void printTasksInfo() {
    sem_wait(&print);
    for (int j = 0; j < tasks_count; ++j) {
        printf("task with id = %d and status = %d, executed by programmer #%d, checked by programmer %d\n",
               tasks[j].id,
               tasks[j].status,
               tasks[j].executor_id,
               tasks[j].checker_id);
    }
    sem_post(&print);
}

void getWork(struct response *response, int programmer_id) {

    for (int i = 0; i < tasks_count; ++i) {
        if (tasks[i].status == NEW) {
            printf("programmer #%d has found task with id = %d for executing\n", programmer_id, tasks[i].id);
            response->response_code = NEW_TASK;
            tasks[i].executor_id = programmer_id;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        } else if (tasks[i].status == FIX && tasks[i].executor_id == programmer_id) {
            printf("programmer #%d is fixing task with id = %d\n", programmer_id, tasks[i].id);
            response->response_code = NEW_TASK;
            tasks[i].status = EXECUTING;
            response->task = tasks[i];
            return;
        } else if (tasks[i].status == EXECUTED && tasks[i].executor_id != programmer_id) {
            printf("programmer #%d has found task with id = %d for checking\n", programmer_id, tasks[i].id);
            response->response_code = CHECK_TASK;
            tasks[i].checker_id = programmer_id;
            tasks[i].status = CHECKING;
            response->task = tasks[i];
            return;
        } else if (tasks[i].executor_id == programmer_id && tasks[i].status == WRONG) {
            printf("programmer #%d need to fix his task with id = %d\n", programmer_id, tasks[i].id);
            response->response_code = FIX_TASK;
            tasks[i].status = FIX;
            response->task = tasks[i];
            return;
        } else {
            printTasksInfo();
        }
    }

    if (complete_count == tasks_count) {
        response->response_code = FINISH;
    }
}

void sendTask(struct task *task) {
    task->status = EXECUTED;
    tasks[task->id] = *task;
}

void sendCheckResult(struct task *task) {
    tasks[task->id] = *task;
    if (task->status == RIGHT) {
        ++complete_count;
        printf("\nComplete count = %d\n", complete_count);
    }
}

int handleClientRequest(int clntSocket, struct request *request) {

    struct task null_task = {-1, -1, -1, -1};
    struct response response = {-1, null_task};

    sem_wait(&sem);

    if (complete_count == tasks_count) {
        response.response_code = FINISH;

        printf("\n\nFINISH\n\n");
        printTasksInfo();
    } else {
        int programmer_id = request->programmer_id;
        struct task task = request->task;

        switch (request->request_code) {
            case 0:getWork(&response, programmer_id);
                break;
            case 1:sendTask(&task);
                getWork(&response, programmer_id);
                break;
            case 2:sendCheckResult(&task);
                getWork(&response, programmer_id);
                break;
            default:break;
        }
    }

    sem_post(&sem);

    send(clntSocket, &response, sizeof(response), 0);

    return response.response_code;
}


void HandleTCPClient(int clntSocket) {
    while (1) {
        struct request request = {-1, -1, -1};

        if (recv(clntSocket, &request, sizeof(request), 0) < 0) {
            perror("receive bad");
            exit(1);
        }
        if (handleClientRequest(clntSocket, &request) == FINISH) {
            break;
        }
    }

    close(clntSocket);
}

int main(int argc, char *argv[]) {
    int servSock;
    int clntSock;
    unsigned short echoServPort;
    pthread_t threadID;
    struct ThreadArgs *threadArgs;

    sem_init(&sem, 0, 1);
    sem_init(&print, 0, 1);

    echoServPort = atoi(argv[1]);
    tasks_count = atoi(argv[2]);

    initPulls();

    servSock = CreateTCPServerSocket(echoServPort);

    while (complete_count < tasks_count) {
        clntSock = AcceptTCPConnection(servSock);

        if ((threadArgs = (struct ThreadArgs *) malloc(sizeof(struct ThreadArgs))) == NULL) {
            perror("malloc() failed");
            exit(1);
        }
        threadArgs->clntSock = clntSock;

        if (pthread_create(&threadID, NULL, ThreadMain, (void *) threadArgs) != 0) {
            perror("pthread_create()failed");
            exit(1);
        }
        printf("with thread %ld\n", (long int) threadID);
    }

    printf("reached!!!\n");

    printf("\n\nFINISH FINISH FINISH\n\n");
    printTasksInfo();

    sem_destroy(&sem);
    sem_destroy(&print);

    close(servSock);
}
