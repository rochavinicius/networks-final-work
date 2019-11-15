#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "macros.h"
#include "utils.h"

struct ServerInfo *startServer()
{
    struct ServerInfo *si = (struct ServerInfo *)malloc(sizeof(struct ServerInfo));

    printf("Starting server service...\n");

    // Creating socket file descriptor
    if ((si->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Server socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("Server socket created.\n");

    // Filling server information
    si->sockaddr.sin_family = AF_INET; // IPv4
    si->sockaddr.sin_addr.s_addr = INADDR_ANY;
    si->sockaddr.sin_port = htons(SERVER_PORT);

    // Bind the socket with the server address
    if (bind(si->socket, (const struct sockaddr *)&si->sockaddr,
             sizeof(si->sockaddr)) < 0)
    {
        perror("Server bind failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("Server binded on port: %d.\n", SERVER_PORT);
    printf("Server started. Waiting for incoming connections...\n");

    return si;
}

void executeStopAndWait(int size, int windowSize)
{
    FILE *file;

    file = fopen("receivedFile.txt", "w");

    if (file == NULL)
    {
        perror("Error while creating received file.\n");
        exit(1);
    }

    while (size > 0)
    {
        // receive
        // check errors
        // send ack
        file -= windowSize;
    }

    fclose(file);
}

void executeGoBackN(int size, int windowSize)
{
    FILE *file;

    file = fopen("receivedFile.txt", "w");

    if (file == NULL)
    {
        perror("Error while creating received file.\n");
        exit(1);
    }

    while (size > 0)
    {
        // receive
        // check errors
        // send ack
        file -= windowSize;
    }

    fclose(file);
}

// gcc server.c -o server
// ./server
void main()
{
    int newSocket;
    char inputData[MAX_BUFFER_SIZE];
    struct sockaddr_in client_addr;

    //TODO trocar para o tal do pontero
    struct ServerInfo *serverInfo = startServer();

    int c = sizeof(struct sockaddr_in);
    while ((newSocket = recvfrom(serverInfo->socket, inputData, sizeof(inputData), MSG_WAITALL,
                                 (struct sockaddr *)&client_addr, (socklen_t *)&c)) > 0)
    {
        printf("Connection accepted\n");

        // ******************* INITIAL PACKAGE *******************

        // for connection stablishment package, do not use any flow control technique
        // just drop it case its with error
        unsigned int crcRet = crc8x_fast(CRC_POLYNOME, inputData, sizeof(inputData));
        if (crcRet != 0)
            continue;

        // Parse main pakage
        //TODO faze o free du bagui
        struct Package *package = parseToPackage(inputData);

        // wrong connection stablishment package
        //TODO verificar o campo Tamanho ao inves de ler os dados
        if (strlen(package->data) != 9 || package->type != 3)
            continue;

        // parse connection stablishment package
        int flowControl;
        int windowSize;
        int fileSize;

        char aux[5];

        memcpy(aux, &package->data[0], 4);
        aux[4] = '\0';
        flowControl = atoi(aux);

        windowSize = package->data[4] - '0';

        memcpy(aux, &package->data[5], 4);
        aux[4] = '\0';
        fileSize = atoi(aux);

        if (flowControl != 1 && flowControl != 0)
            continue;
        if (windowSize < 1 || windowSize > 16)
            continue;

        // send connection stablished ack
        struct Package pkg;
        pkg.destiny = package->source;
        pkg.source = package->destiny;
        pkg.type = 1;
        pkg.sequency = 0;
        pkg.crc = 0;
        pkg.crc = crc8x_fast(CRC_POLYNOME, (const void *)&pkg, sizeof(pkg));

        sendto(serverInfo->socket, (const void *)&pkg, sizeof(pkg), 0,
                         (const struct sockaddr *)&serverInfo->sockaddr,
                         sizeof(struct sockaddr));

        //TODO fazer a comunicacao utilizando a tecnica de controle de fluxo
        if (flowControl == 0)
            executeStopAndWait(fileSize, windowSize);
        else
            executeGoBackN(fileSize, windowSize);

        // sends final ack to finish execution
        //TODO fazer ack final
    }

    if (newSocket < 0)
    {
        perror("Error recvfrom server.\n");
        exit(EXIT_FAILURE);
    }

    free(serverInfo);
}