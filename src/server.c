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

void executeStopAndWait(int fileSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source);
void executeGoBackN(int size, int windowSize);

// gcc utils.c server.c -o server
// ./server
void main()
{
    int newSocket;
    struct Package buffer;

    struct ServerInfo *serverInfo = startServer();

    int c = sizeof(struct sockaddr_in);
    while ((newSocket = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                                 (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c)) > 0)
    {
        printf("Connection accepted\n");

        // ******************* INITIAL PACKAGE *******************

        parseNetworkToPackage(&buffer);

        // for connection stablishment package, do not use any flow control technique
        // just drop it case its with error
        unsigned int crcRet = crc32b((unsigned char *)&buffer, CRC_POLYNOME);

        printf("CRC calculated for package: %d.\n", crcRet);
        printf("CRC recebida: %d\n", buffer.crc);

        if (crcRet != buffer.crc)
            continue;

        // Parse main pakage
        struct Package *package = parseToPackage(&buffer);

        // wrong connection stablishment package
        if (package->size < 1 || package->size > 16 || package->type != CONNECTION_TYPE)
        {
            printf("Wrong package size or type.\n");
            printf("size: %d   type %d\n", package->size, package->type);
            free(package);
            continue;
        }

        // parse connection stablishment package
        struct ConnectionData *connectionData = parseToConnectionData(buffer.data);

        // validate for connection stablishment package limits
        if (connectionData->flowControl != 1 && connectionData->flowControl != 0)
        {
            printf("wrong flow control.\n");
            printf("flowcontrol %d.\n", connectionData->flowControl);
            free(connectionData);
            continue;
        }
        if (connectionData->flowControl == 1 && (connectionData->windowSize < 1 || connectionData->windowSize > 16))
        {
            printf("wrong window size.\n");
            printf("windowsize %d.\n", connectionData->windowSize);
            free(connectionData);
            continue;
        }

        // create connection stablished ACK package
        struct Package ackPackage;
        ackPackage.destiny = package->source;
        ackPackage.source = package->destiny;
        ackPackage.type = 1;
        ackPackage.sequency = 0;
        ackPackage.crc = 0;
        ackPackage.crc = crc32b((unsigned char *)&ackPackage, CRC_POLYNOME);

        parsePackageToNetwork(&ackPackage);

        printf("Sending ack to client...\n");

        // send ACK package
        sendto(serverInfo->socket, (const void *)&ackPackage, sizeof(ackPackage), 0,
               (const struct sockaddr *)&serverInfo->clientaddr,
               sizeof(struct sockaddr));

        printf("Sent ack response to client.\n");

        if (connectionData->flowControl == 0)
            executeStopAndWait(connectionData->fileSize, serverInfo, package->destiny, package->size);
        else
            executeGoBackN(connectionData->fileSize, connectionData->windowSize);

        free(connectionData);
        free(package);

        printf("\n\n");
        printf("File transmission finished.\n");
        printf("Waiting for incomming connections...\n");
        printf("\n\n");
    }

    close(newSocket);
    close(serverInfo->socket);
    free(serverInfo);

    if (newSocket < 0)
    {
        perror("Error recvfrom server.\n");
        exit(EXIT_FAILURE);
    }
}

void executeStopAndWait(int fileSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source)
{
    FILE *file;
    struct Package *receivedPackage;
    struct Package package;
    struct Package buffer;
    int c;
    int retRecv;
    int retSend;
    int sequency = 0;
    unsigned int crc;

    printf("Starting Stop and Wait protocol...\n");

    printf("received file size %d\n", fileSize);

    // file were the reiceved file will be saved
    file = fopen("receivedFile.txt", "w");
    if (file == NULL)
    {
        perror("Error while creating received file in Stop and Wait.\n");
        exit(1);
    }

    printf("File size initial: %d\n", fileSize);

    while (fileSize > 0)
    {
        memset(&buffer, 0, sizeof(buffer));

        retRecv = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c);
        if (retRecv < 0)
        {
            perror("Error getting package from client. Stop and Wait\n");
            exit(1);
        }

        parseNetworkToPackage(&buffer);

        printf("Received package from client.\n");

        receivedPackage = parseToPackage(&buffer);

        // check for CRC
        crc = crc32b((unsigned char *)&buffer, CRC_POLYNOME);

        // bitflip, ask for retransmission
        while (crc != buffer.crc)
        {
            free(receivedPackage);

            package.destiny = destiny;
            package.source = source;
            package.type = ACK_TYPE;
            package.sequency = sequency;

            parsePackageToNetwork(&package);

            retSend = sendto(serverInfo->socket, (const void *)&package, sizeof(package), 0,
                             (const struct sockaddr *)&serverInfo->clientaddr,
                             sizeof(struct sockaddr));
            if (retSend < 0)
            {
                perror("Error sending package to client. Stop and Wait\n");
                exit(1);
            }

            retRecv = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c);
            if (retRecv < 0)
            {
                perror("Error getting package from client. Stop and Wait\n");
                exit(1);
            }

            parseNetworkToPackage(&buffer);

            receivedPackage = parseToPackage(&buffer);

            // check for CRC
            crc = crc32b((unsigned char *)&buffer, CRC_POLYNOME);
        }

        // write to file
        fwrite(receivedPackage->data, sizeof(receivedPackage->data[0]),
               receivedPackage->size, file);

        // decrement file size
        fileSize -= receivedPackage->size;

        printf("File size: %d, size received %d\n", fileSize, receivedPackage->size);

        sequency++;
        if (sequency > 1)
            sequency = 0;

        package.destiny = destiny;
        package.source = source;
        package.sequency = sequency;
        package.type = ACK_TYPE;

        parsePackageToNetwork(&package);

        printf("Requesting next frame to be transmitted.\n");

        // send ACK for next frame
        retSend = sendto(serverInfo->socket, (const void *)&package, sizeof(package), 0,
                         (const struct sockaddr *)&serverInfo->clientaddr,
                         sizeof(struct sockaddr));
        if (retSend < 0)
        {
            perror("Error sendto requesting next frame.\n");
            exit(1);
        }

        printf("Request sent.\n");

        free(receivedPackage);
    }

    if (sequency > 1)
        sequency = 0;

    package.destiny = destiny;
    package.source = source;
    package.sequency = sequency;
    package.type = ACK_TYPE;

    parsePackageToNetwork(&package);

    printf("Sending final ack for terminating.\n");

    // Sends final ack for terminating
    retSend = sendto(serverInfo->socket, (const void *)&package, sizeof(package), 0,
                     (const struct sockaddr *)&serverInfo->clientaddr,
                     sizeof(struct sockaddr));
    if (retSend < 0)
    {
        perror("Error sendto terminating ack.\n");
        exit(1);
    }

    printf("Final ack sent.\n");

    fclose(file);
}

//TODO fazer esse cara baseado no client
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