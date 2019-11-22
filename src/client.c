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

struct ClientInfo *startClient(char ip[], int port)
{
    struct ClientInfo *clientInfo = (struct ClientInfo *)malloc(sizeof(struct ClientInfo));
    int clientSocket;
    struct sockaddr_in server_addr;
    int hostName;
    char hostBuffer[256];
    struct hostent *hostEntry;

    printf("Starting client service...\n");

    // Creating socket file descriptor
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Client socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("Client socket created.\n");

    // Filling server information
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);

    printf("Client started. Ready to send data...\n");

    clientInfo->socket = clientSocket;
    clientInfo->sockaddr = server_addr;

    hostName = gethostname(hostBuffer, sizeof(hostBuffer));
    hostEntry = gethostbyname(hostBuffer);
    clientInfo->clientIp = inet_ntoa(*(struct in_addr *)hostEntry->h_addr_list[0]);

    return clientInfo;
}

char *getValidFile()
{
    FILE *file;
    char *fileName = NULL;
    size_t len = 0;
    int read;
    bool isValidFile = false;

    printf("Enter file name or path to send to server.\n");
    printf("Type 'exit' to exit program.\n");

    while (!isValidFile)
    {
        printf("REDESI@file$ ");

        read = getline(&fileName, &len, stdin);

        fileName[read - 1] = '\0';

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
// void executeStopAndWait(int fileSize, char fileName[], struct ClientInfo *clientInfo);
// void executeGoBackN(int fileSize, char fileName[], struct ClientInfo *clientInfo, int windowSize);

//TODO adicionar o parametro para o server PORT
void getCommandLineArgs(int args, char **argc, int *flowControl, int *windowSize, char ip[])
{
    int opt;
    bool wFlag = false;
    bool iFlag = false;
    bool ipInformed = false;

    while ((opt = getopt(args, argc, ":f:ws:ip:")) != -1)
    {
        switch (opt)
        {
        case 'f':
            *flowControl = atoi(optarg);
            break;
        case 'w':
            wFlag = true;
            break;
        case 's':
            if (!wFlag)
            {
                printf("Wrong argument: -%c. The correct argument is -ws.\n", opt);
                exit(1);
            }
            *windowSize = atoi(optarg);
            break;
        case 'i':
            iFlag = true;
            break;
        case 'p':
            if (!iFlag)
            {
                printf("Wrong argument: -%c. The correct argument is -ip.\n", opt);
                exit(1);
            }
            memcpy(ip, optarg, 12);
            ipInformed = true;
            break;
        case '?':
            printf("Unknown arguments: -%c\n", optopt);
            exit(1);
        }
    }

    if (!ipInformed)
        memcpy(ip, SERVER_IP, sizeof(SERVER_IP));
    ip[strlen(ip)] = '\0';
}

// gcc utils.c client.c -o client
// ./client -f 0 -ws 5 -ip 127.0.0.1
// ./client -f {flowControl} -ws {windowSize} -ip {server ip}
void main(int args, char **argc)
{
    int newSocket;
    char buffer[MAX_BUFFER_SIZE];
    long int fileSize;
    int flowControl;
    int windowSize;
    char ip[13];
    int c = sizeof(struct sockaddr_in);

    // Parse command line arguments required for client program
    getCommandLineArgs(args, argc, &flowControl, &windowSize, ip);

    // Start connection with server
    struct ClientInfo *clientInfo = startClient(ip, SERVER_PORT);

    while (1)
    {
        FILE *file;
        char *fileName;
        bool isValidFile = false;
        struct Package buffer;
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
        package.crc = crc32b((unsigned char *)&package, CRC_POLYNOME);

        printf("CRC calculated %d.\n", package.crc);

        parsePackageToNetwork(&package);

        retSend = sendto(clientInfo->socket, (const void *)&package, sizeof(struct Package),
                         0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));

        if (retSend < 0)
        {
            perror("Error sending package to server.\n");
            exit(EXIT_FAILURE);
        }

        printf("Sent package to server.\n");

        // get confirmation ack from server
        retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                           0, (struct sockaddr *)&clientInfo->sockaddr,
                           &c);

        if (retRecv <= 0)
        {
            perror("Error recvfrom <= 0\n");
            exit(1);
        }

        parseNetworkToPackage(&buffer);

        printf("Received ack from server.\n");

        struct Package *ackPackage = &buffer;

        if (ackPackage->type != ACK_TYPE)
        {
            perror("Server response not acknoledge.\n");
            continue;
        }

        if (flowControl == 0)
            clientStopAndWait(fileSize, fileName, clientInfo);
        else
            clientGoBackN(fileSize, fileName, clientInfo, windowSize);

        free(fileName);
        // free(ackPackage);

        printf("\n\n");
        printf("File transmission finished.\n");
        printf("\n\n");
    }

    printf("Client socket connection closed. Finishing...");
}

/*void executeStopAndWait(int fileSize, char fileName[], struct ClientInfo *clientInfo)
{
    FILE *file = fopen(fileName, "r");
    char dataBuffer[240];
    int nrBytesRead;
    struct Package package;
    struct Package *ackPackage;
    struct Package buffer;
    int retSend;
    int retRecv;
    int sequency = 0;
    int incrementedSequency;
    int c = sizeof(struct sockaddr_in);

    package.destiny = clientInfo->sockaddr.sin_addr.s_addr;
    package.source = inet_addr(clientInfo->clientIp);
    package.type = DATA_TYPE;

    printf("Starting Stop and Wait protocol...\n");

    while (fileSize > 0)
    {
        if (sequency > 1)
            sequency = 0;

        // read file and get the number of bytes read (in case the file ends)
        nrBytesRead = fread(dataBuffer, 1, sizeof(dataBuffer), file);
        if (nrBytesRead < 0)
        {
            perror("Error reading file.\n");
            exit(1);
        }
        printf("Read %d bytes from file.\n", nrBytesRead);

        // Set data, size, sequency and CRC
        memcpy(package.data, dataBuffer, nrBytesRead);
        package.size = nrBytesRead;
        package.sequency = sequency;
        package.crc = 0;
        package.crc = crc32b((unsigned char *)&package, CRC_POLYNOME);

        parsePackageToNetwork(&package);

        printf("Sending package to server.\n");
        printf("Sending size %d\n", package.size);

        // send frame to server
        retSend = sendto(clientInfo->socket, (const void *)&package, sizeof(struct Package),
                         0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
        if (retSend < 0)
        {
            perror("Error sending package in Stop and Wait.\n");
            exit(1);
        }

        printf("Package sent to server.\n");

        printf("Waiting for server ack.\n");

        // waits for ack form the server
        retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                           0, (struct sockaddr *)&clientInfo->sockaddr,
                           &c);
        if (retRecv < 0)
        {
            perror("Error recvfrom <= 0 Stop and Wait.\n");
            exit(1);
        }

        parseNetworkToPackage(&buffer);

        printf("Server ack received.\n");

        ackPackage = &buffer;

        if (ackPackage->type != ACK_TYPE)
        {
            perror("Server response not acknoledge Stop and Wait.\n");
            exit(1);
        }

        incrementedSequency = sequency + 1 > 1 ? 0 : 1;

        printf("ACK SEQUENCY %d\n", ackPackage->sequency);

        while (ackPackage->sequency != incrementedSequency)
        {
            printf("Sending frame retransmission to server.\n");

            // free(ackPackage);

            // Do data retransmission
            retSend = sendto(clientInfo->socket, (const void *)&package, sizeof(struct Package),
                             0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
            if (retSend <= 0)
            {
                perror("Error sending package retransmission in Stop and Wait.\n");
                exit(1);
            }

            // waits for ack form the server
            retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                               0, (struct sockaddr *)&clientInfo->sockaddr,
                               &c);
            if (retRecv <= 0)
            {
                perror("Error recvfrom <= 0 Stop and Wait.\n");
            }

            parseNetworkToPackage(&buffer);

            ackPackage = &buffer;
            if (ackPackage->type != ACK_TYPE)
            {
                perror("Server response not acknoledge Stop and Wait.\n");
                exit(1);
            }
        }

        // free(ackPackage);
        sequency = sequency == 0 ? 1 : 0;
        fileSize -= nrBytesRead;
    }

    printf("Waiting for terminating ACK.\n");

    // Waits for the final ack
    retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                       0, (struct sockaddr *)&clientInfo->sockaddr,
                       &c);
    if (retRecv <= 0)
    {
        perror("Error recvfrom for final ack. Stop and Wait.\n");
        exit(1);
    }

    printf("Protocol finished.\n");

    fclose(file);
}

//TODO comentar o codigo, arrumar os prints de log, c pa coloca algumas coisa em funcao
void executeGoBackN(int fileSize, char fileName[], struct ClientInfo *clientInfo, int windowSize)
{
    FILE *file = fopen(fileName, "r");

    char dataBuffer[240];
    int nrBytesRead;
    struct Package *ackPackage;
    struct Package buffer;
    struct Package *frames = (struct Package *)malloc(windowSize * sizeof(struct Package));
    int retSend;
    int retRecv;
    int sequency = 0;
    int incrementedSequency;
    int c = sizeof(struct sockaddr_in);
    int i;
    bool backToBegin = false;

    printf("Waiting for terminating ACK.\n");

    while (fileSize > 0)
    {
        // Send all the window frames
        for (i = 0; i < windowSize; i++)
        {
            if (fileSize <= 0)
                break;

            // read file and get the number of bytes read (in case the file ends)
            nrBytesRead = fread(dataBuffer, 1, sizeof(dataBuffer), file);
            if (nrBytesRead < 0)
            {
                perror("Error reading file.\n");
                exit(1);
            }

            fileSize -= nrBytesRead;

            // Populate Package
            frames[sequency].destiny = clientInfo->sockaddr.sin_addr.s_addr;
            frames[sequency].source = inet_addr(clientInfo->clientIp);
            frames[sequency].type = DATA_TYPE;
            memcpy(frames[sequency].data, dataBuffer, nrBytesRead);
            frames[sequency].size = nrBytesRead;
            frames[sequency].sequency = sequency;
            frames[sequency].crc = 0;
            frames[sequency].crc = crc32b((unsigned char *)&frames[sequency], CRC_POLYNOME);

            printf("CRC calculated: %d\n", frames[sequency].crc);

            parsePackageToNetwork(&frames[sequency]);

            printf("Sending package to server.\n");

            // Send package to server
            retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                             0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
            if (retSend < 0)
            {
                perror("Error sending package in Stop and Wait.\n");
                exit(1);
            }
            sequency = sequency == windowSize - 1 ? 0 : sequency++;
        }

        // waits for ack form the server
        retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                           0, (struct sockaddr *)&clientInfo->sockaddr,
                           &c);
        if (retRecv < 0)
        {
            perror("Error recvfrom <= 0 Stop and Wait.\n");
            exit(1);
        }

        parseNetworkToPackage(&buffer);

        ackPackage = &buffer;

        // basic validation for server response: it must be an ack
        if (ackPackage->type != ACK_TYPE)
        {
            perror("Server response not acknoledge Stop and Wait.\n");
            exit(1);
        }

        printf("ACK SEQUENCY %d\n", ackPackage->sequency);

        incrementedSequency = sequency == windowSize - 1 ? 0 : sequency++;

        // server requesting frame retransmission
        // send all the frames that were dropped from server
        while (incrementedSequency != ackPackage->sequency)
        {
            sequency = ackPackage->sequency;

            printf("Sending frame retransmission to server.\n");

            for (i = 0; i < windowSize; i++)
            {
                if (backToBegin)
                {
                    if (fileSize <= 0)
                        break;

                    // read file and get the number of bytes read (in case the file ends)
                    nrBytesRead = fread(dataBuffer, 1, sizeof(dataBuffer), file);
                    if (nrBytesRead < 0)
                    {
                        perror("Error reading file.\n");
                        exit(1);
                    }

                    fileSize -= nrBytesRead;

                    // Populate Package
                    frames[sequency].destiny = clientInfo->sockaddr.sin_addr.s_addr;
                    frames[sequency].source = inet_addr(clientInfo->clientIp);
                    frames[sequency].type = DATA_TYPE;
                    memcpy(frames[sequency].data, dataBuffer, nrBytesRead);
                    frames[sequency].size = nrBytesRead;
                    frames[sequency].sequency = sequency;
                    frames[sequency].crc = 0;
                    frames[sequency].crc = crc32b((unsigned char *)&frames[sequency], CRC_POLYNOME);

                    parsePackageToNetwork(&frames[sequency]);

                    retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                                     0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
                    if (retSend <= 0)
                    {
                        perror("Error sending package retransmission in Stop and Wait.\n");
                        exit(1);
                    }
                }
                else
                {
                    // Do data retransmission
                    retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                                     0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
                    if (retSend <= 0)
                    {
                        perror("Error sending package retransmission in Stop and Wait.\n");
                        exit(1);
                    }
                }

                sequency++;
                if (sequency == windowSize - 1)
                {
                    sequency = 0;
                    backToBegin = true;
                }
            }

            // waits for ack form the server
            retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                               0, (struct sockaddr *)&clientInfo->sockaddr,
                               &c);
            if (retRecv <= 0)
            {
                perror("Error recvfrom <= 0 Stop and Wait.\n");
            }

            parseNetworkToPackage(&buffer);

            ackPackage = &buffer;
            if (ackPackage->type != ACK_TYPE)
            {
                perror("Server response not acknoledge Stop and Wait.\n");
                exit(1);
            }
        }

        // No frames with errors
    }

    printf("Waiting for terminating ACK.\n");

    // Waits for the final ack
    retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                       0, (struct sockaddr *)&clientInfo->sockaddr,
                       &c);
    if (retRecv <= 0)
    {
        perror("Error recvfrom for final ack. Stop and Wait.\n");
        exit(1);
    }

    printf("Protocol finished.\n");

    fclose(file);
}*/
