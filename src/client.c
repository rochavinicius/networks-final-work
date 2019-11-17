#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <netdb.h>

#include "macros.h"
#include "utils.h"

struct ClientInfo *startClient()
{
    struct ClientInfo *clientInfo = (struct ClientInfo *)malloc(sizeof(struct ClientInfo));
    int clientSocket;
    struct sockaddr_in server_addr;
    int hostName;
    char hostBuffer[256];
    struct hostent *hostEntry;

    printf("Starting client service...");

    // Creating socket file descriptor
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Client socket creation failed.");
        exit(EXIT_FAILURE);
    }

    printf("Client socket created.");

    // Filling server information
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    printf("Client started. Ready to send data...");

    clientInfo->socket = clientSocket;
    clientInfo->sockaddr = server_addr;

    hostName = gethostname(hostBuffer, sizeof(hostBuffer));
    hostEntry = gethostbyname(hostBuffer);
    clientInfo->clientIp = inet_ntoa(*(struct in_addr *)hostEntry->h_addr_list[0]);

    return clientInfo;
}

char * getValidFile()
{
    FILE *file;
    char *fileName = (char *)malloc(256 * sizeof(char));
    bool isValidFile = false;

    while (!isValidFile)
    {
        printf("Enter file name or path to send to server.\n");
        printf("Type 'exit' to exit program.\n");
        printf("REDESI@file$ ");

        fgets(fileName, sizeof(fileName), stdin);

        if (strcmp(fileName, "exit") == 0)
            exit(0);

        file = fopen(fileName, "r");

        if (file != NULL)
        {
            isValidFile = true;
            fclose(file);
        }
        else
        {
            printf("File does not exists.\n");
            sleep(1);
        }
    }

    return fileName;
}

// function macros
void executeStopAndWait(int fileSize, char fileName[], struct ClientInfo *clientInfo);
void executeGoBackN(int fileSize, int windowSize, char fileName[]);

// gcc client.c -o client
// ./client
void main(int args, char **argc)
{
    int newSocket;
    char buffer[MAX_BUFFER_SIZE];
    long int fileSize;

    //TODO obter as informacoes por parametro, como windowSize e qual o controle de fluxo
    int flowControl = 0;
    int windowSize = 8;

    struct ClientInfo *clientInfo = startClient();

    while (1)
    {
        FILE *file;
        char *fileName;
        bool isValidFile = false;
        char inputData[MAX_BUFFER_SIZE];
        ssize_t retSend;
        ssize_t retRecv;
        struct stat st;

        // get input file that will be sent to server
        fileName = getValidFile();

        stat(fileName, &st);
        fileSize = st.st_size;

        // send package to stablish connection with server

        struct ConnectionData data;
        data.fileSize = fileSize;
        data.flowControl = flowControl;
        data.windowSize = windowSize;

        int clientIpFormatted = inet_addr(clientInfo->clientIp);

        struct Package package;
        package.destiny = clientInfo->sockaddr.sin_addr.s_addr;
        package.source = clientIpFormatted;
        package.type = CONNECTION_TYPE;
        package.sequency = 0;
        package.size = sizeof(struct ConnectionData);
        memcpy(package.data, &data, sizeof(struct ConnectionData));
        package.crc = 0;
        package.crc = crc8x_fast(CRC_POLYNOME, (const void *)&package, sizeof(struct Package));

        retSend = sendto(clientInfo->socket, (const void *)&package, sizeof(struct Package),
                         0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));

        if (retSend < 0)
        {
            perror("Error sending package to server.\n");
            exit(EXIT_FAILURE);
        }

        // get confirmation ack from server
        retRecv = recvfrom(clientInfo->socket, inputData, sizeof(inputData),
                           MSG_WAITALL, (struct sockaddr *)&clientInfo->sockaddr,
                           &clientInfo->socket);

        if (retRecv <= 0)
        {
            perror("Error recvfrom <= 0\n");
        }

        struct Package *ackPackage = parseToPackage(inputData);

        if (ackPackage->type != ACK_TYPE)
        {
            perror("Server response not acknoledge.\n");
            continue;
        }

        if (flowControl == 0)
            executeStopAndWait(fileSize, fileName, clientInfo);
        else
            executeGoBackN(fileSize, windowSize, fileName);

        //TODO esperar ack final ou colocar dentro dos metodos

        free(fileName);
        free(ackPackage);
        free(clientInfo);

        close(clientInfo->socket);
        fclose(file);
    }

    printf("Client socket connection closed. Finishing...");
}

//TODO use fread to read the file since it returns the number characters read from the file

void executeStopAndWait(int fileSize, char fileName[], struct ClientInfo *clientInfo)
{
    FILE *file = fopen(fileName, "r");
    char dataBuffer[240];
    int nrBytesRead;
    struct Package package;
    struct Package *ackPackage;
    char inputData[MAX_BUFFER_SIZE];
    int retSend;
    int retRecv;
    int sequency = 0;
    int incrementedSequency;

    package.destiny = clientInfo->sockaddr.sin_addr.s_addr;
    package.source = inet_addr(clientInfo->clientIp);
    package.type = DATA_TYPE;

    while (fileSize > 0)
    {
        if (sequency > 1)
            sequency = 0;

        // read file and get the number of bytes read (in case the file ends)
        nrBytesRead = fread(dataBuffer, 1, sizeof(dataBuffer), file);

        // Set data, size, sequency and CRC
        package.size = nrBytesRead;
        memcpy(package.data, dataBuffer, nrBytesRead);
        package.sequency = sequency;
        package.crc = 0;
        package.crc = crc8x_fast(CRC_POLYNOME, &package, sizeof(package));

        // send frame to server
        retSend = sendto(clientInfo->socket, (const void *)&package, sizeof(struct Package),
                         0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
        if (retSend <= 0)
        {
            perror("Error sending package in Stop and Wait.\n");
            exit(1);
        }

        // waits for ack form the server
        retRecv = recvfrom(clientInfo->socket, inputData, sizeof(inputData),
                           MSG_WAITALL, (struct sockaddr *)&clientInfo->sockaddr,
                           &clientInfo->socket);
        if (retRecv <= 0)
        {
            perror("Error recvfrom <= 0 Stop and Wait.\n");
        }

        ackPackage = parseToPackage(inputData);

        if (ackPackage->type != ACK_TYPE)
        {
            perror("Server response not acknoledge Stop and Wait.\n");
            exit(1);
        }

        incrementedSequency = sequency + 1 > 1 ? 0 : 1;

        while (ackPackage->sequency != incrementedSequency)
        {
            free(ackPackage);

            // Do data retransmission
            retSend = sendto(clientInfo->socket, (const void *)&package, sizeof(struct Package),
                             0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
            if (retSend <= 0)
            {
                perror("Error sending package in Stop and Wait.\n");
                exit(1);
            }

            // waits for ack form the server
            retRecv = recvfrom(clientInfo->socket, inputData, sizeof(inputData),
                               MSG_WAITALL, (struct sockaddr *)&clientInfo->sockaddr,
                               &clientInfo->socket);
            if (retRecv <= 0)
            {
                perror("Error recvfrom <= 0 Stop and Wait.\n");
            }

            ackPackage = parseToPackage(inputData);
        }

        sequency++;
    }

    fclose(file);
}

void executeGoBackN(int fileSize, int windowSize, char fileName[])
{
}