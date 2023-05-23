#include "enums.h"

#define BUFFER_SIZE 1024

int main(int argc, char *argv[])
{
    int sock;
    struct sockaddr_in echoServAddr;
    unsigned short echoServPort;
    char *servIP;

    servIP = "127.0.0.1";

    if ((argc < 2) || (argc > 3))
    {
        fprintf(stderr, "Usage: %s <Server IP> <Echo Port>\n",
                argv[0]);
    }
    else
    {
        servIP = argv[1];
    }

    if (argc == 3)
        echoServPort = atoi(argv[2]);
    else
        echoServPort = 8000;

    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("socket failed");
        exit(1);
    }

    memset(&echoServAddr, 0, sizeof(echoServAddr));
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);
    echoServAddr.sin_port = htons(echoServPort);

    char buffer[BUFFER_SIZE];

    if (connect(sock, (struct sockaddr *)&echoServAddr, sizeof(echoServAddr)) < 0) {
        perror("connect failed");
        exit(1);
    }

    while (1)
    {
        bzero(buffer, BUFFER_SIZE);
        strcpy(buffer, "HELLO, THIS IS CLIENT.");
        printf("Client: %s\n", buffer);
        send(sock, buffer, strlen(buffer), 0);

        bzero(buffer, BUFFER_SIZE);
        recv(sock, buffer, sizeof(buffer), 0);
        printf("Got from server: %s\n", buffer);
    }

    close(sock);
    printf("Disconnected from the server.\n");
    exit(0);
}
