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

void executeStopAndWait(int fileSize, struct ServerInfo *serverInfo);
void executeGoBackN(int size, int windowSize);

// gcc server.c -o server
// ./server
void main()
{
    int newSocket;
    char inputData[MAX_BUFFER_SIZE];
    struct sockaddr_in client_addr;

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
        struct Package *package = parseToPackage(inputData);

        // wrong connection stablishment package
        if (package->size < 1 || package->size > 16 || package->type != 3)
        {
            free(package);
            continue;
        }

        // parse connection stablishment package
        struct ConnectionData *connectionData = parseToConnectionData(inputData);

        // validate for connection stablishment package limits
        if (connectionData->flowControl != 1 && connectionData->flowControl != 0)
        {
            free(connectionData);
            continue;
        }
        if (connectionData->windowSize < 1 || connectionData->windowSize > 16)
        {
            free(connectionData);
            continue;
        }

        // send connection stablished ACK
        struct Package ackPackage;
        ackPackage.destiny = package->source;
        ackPackage.source = package->destiny;
        ackPackage.type = 1;
        ackPackage.sequency = 0;
        ackPackage.crc = 0;
        ackPackage.crc = crc8x_fast(CRC_POLYNOME, (const void *)&ackPackage, sizeof(ackPackage));

        // send ACK package
        sendto(serverInfo->socket, (const void *)&ackPackage, sizeof(ackPackage), 0,
               (const struct sockaddr *)&serverInfo->sockaddr,
               sizeof(struct sockaddr));

        if (connectionData->flowControl == 0)
            executeStopAndWait(connectionData->fileSize, serverInfo);
        else
            executeGoBackN(connectionData->fileSize, connectionData->windowSize);

        free(connectionData);
        free(package);
        free(serverInfo);

        // sends final ack to finish execution
        //TODO fazer ack final
    }

    if (newSocket < 0)
    {
        perror("Error recvfrom server.\n");
        exit(EXIT_FAILURE);
    }
}

void executeStopAndWait(int fileSize, struct ServerInfo *serverInfo)
{
    FILE *file;

    // file were the reiceved file will be saved
    file = fopen("receivedFile.txt", "w");
    if (file == NULL)
    {
        perror("Error while creating received file in Stop and Wait.\n");
        exit(1);
    }

    while (fileSize > 0)
    {
        // receive
        // check errors
        // send ack
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