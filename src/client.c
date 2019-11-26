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
    int hostName;
    char hostBuffer[256];
    struct hostent *hostEntry;

    printf("Starting client service...\n");

    // Creating socket file descriptor
    if ((clientInfo->socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("Client socket creation failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("Client socket created.\n");

    // Filling server information
    clientInfo->sockaddr.sin_family = AF_INET; // IPv4
    clientInfo->sockaddr.sin_addr.s_addr = inet_addr("10.32.160.141");
    clientInfo->sockaddr.sin_port = htons(port);

    printf("Port: %d  ip: %s \n", port, ip);

    printf("Client started. Ready to send data...\n");

    clientInfo->clientIp = "10.32.160.35";

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

void getCommandLineArgs(int args, char **argc, int *flowControl, int *windowSize, char ip[], int *port, bool *hasErrorInsertion)
{
    int opt;
    bool wFlag = false;
    bool portInformed = false;
    bool ipInformed = false;

    while ((opt = getopt(args, argc, ":f:ws:i:p:e")) != -1)
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
            memcpy(ip, optarg, 12);
            ipInformed = true;
            break;
        case 'p':
            *port = atoi(optarg);
            portInformed = true;
            break;
        case 'e':
            *hasErrorInsertion = true;
            break;
        case '?':
            printf("Unknown arguments: -%c\n", optopt);
            exit(1);
        }
    }

    if (!ipInformed)
        memcpy(ip, SERVER_DEFAULT_IP, sizeof(SERVER_DEFAULT_IP));

    ip[strlen(ip)] = '\0';

    if (!portInformed)
        *port = SERVER_DEFAULT_PORT;
}

// gcc utils.c client.c -o client
// ./client -f 0 -ws 5 -i 127.0.0.1 -p 8888
// ./client -f {flowControl} -ws {windowSize} -i {server ip} -p {server port}
void main(int args, char **argc)
{
    int newSocket;
    char buffer[MAX_BUFFER_SIZE];
    long int fileSize;
    int flowControl;
    int windowSize;
    char ip[13];
    int port;
    bool hasErrorInsertion = false;
    int c = sizeof(struct sockaddr_in);

    // Parse command line arguments required for client program
    getCommandLineArgs(args, argc, &flowControl, &windowSize, ip, &port, &hasErrorInsertion);

    printf("port %d\n", port);

    // Start connection with server
    struct ClientInfo *clientInfo = startClient(ip, port);

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

        if (windowSize > fileSize)
        {
            printf("\nWindow size cannot be bigger than file size.\n");
            printf("Either set a lower window size or send a bigger file.\n\n");
            continue;
        }

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
        package.crc = crc32_of_buffer((const char *)&package, sizeof(struct Package));

        printf("CRC calculated %d.\n", package.crc);

        parsePackageToNetwork(&package);

        retSend = sendto(clientInfo->socket, (const void *)&package, sizeof(package),
                         0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(clientInfo->sockaddr));

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
            clientStopAndWait(fileSize, fileName, clientInfo, hasErrorInsertion);
        else
            clientGoBackN(fileSize, fileName, clientInfo, windowSize, hasErrorInsertion);

        free(fileName);

        printf("\n\n");
        printf("File transmission finished.\n");
        printf("\n\n");
    }

    printf("Client socket connection closed. Finishing...");
}
