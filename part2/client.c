#include "enums.h"


int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in echoServAddr;
    unsigned short echoServPort;
    char *servIP;

    int programmer_id = atoi(argv[1]);
    servIP = argv[2];
    echoServPort = atoi(argv[3]);

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
    {
        perror("socket() failed");
        exit(1);
    }



    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    echoServAddr.sin_port = htons(echoServPort);

    if (connect(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0) {
        perror("connect failed");
        exit(1);
    }

    struct task task = {-1, -1, -1, -1};
    struct request request = {GET_WORK, programmer_id, task};

    while (1) {

        if (send(sock, &request, sizeof(request), 0) < 0)
        {
            perror("send() bad");
            exit(1);
        }
        printf("Programmer #%d has sent his request = %d to server\n", programmer_id, request.request_code);

        struct response response = {-1, -1, -1, -1};
        if (recv(sock, &response, sizeof(response), 0) < 0) {
            perror("recv bad");
            exit(1);
        }
        printf("Programmer #%d has got the responce = %d from server\n", programmer_id, response.response_code);

        if (response.response_code == FINISH) {
            break;
        }

        switch (response.response_code) {
            case UB:
                break;
            case NEW_TASK:
                sleep(1);
                request.task = response.task;
                request.request_code = SEND_TASK;
                break;
            case CHECK_TASK:
                sleep(1);

                int8_t result = rand() % 2;
                printf("the result of checking task with id = %d is %d\n", response.task.id, result);
                response.task.status = result == 0 ? WRONG : RIGHT;

                request.task = response.task;
                request.request_code = SEND_CHECK;
                break;
            case FIX_TASK:
                sleep(1);
                request.task = response.task;
                request.request_code = SEND_TASK;
                break;
            default:
                request.request_code = GET_WORK;
                break;
        }
        sleep(5);
    }

    close(sock);
    exit(0);
}
