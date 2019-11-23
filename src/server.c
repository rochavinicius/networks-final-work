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

struct ServerInfo *startServer(int port)
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
    si->sockaddr.sin_port = htons(port);

    // Bind the socket with the server address
    if (bind(si->socket, (const struct sockaddr *)&si->sockaddr,
             sizeof(si->sockaddr)) < 0)
    {
        perror("Server bind failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("Server binded on port: %d.\n", port);
    printf("Server started. Waiting for incoming connections...\n");

    return si;
}

void getCommandLineArgs(int args, char **argc, int *port)
{
    int opt;
    bool portInformed = false;

    while ((opt = getopt(args, argc, ":p:")) != -1)
    {
        switch (opt)
        {
        case 'p':
            *port = atoi(optarg);
            portInformed = true;
            break;
        case '?':
            printf("Unknown arguments: -%c\n", optopt);
            exit(1);
        }
    }

    if (!portInformed)
        *port = SERVER_DEFAULT_PORT;
}

// gcc utils.c server.c -o server
// ./server -p 8888
void main(int args, char **argc)
{
    int newSocket;
    struct Package buffer;
    int port;

    // Parse command line arguments required for server program
    getCommandLineArgs(args, argc, &port);

    struct ServerInfo *serverInfo = startServer(port);

    int c = sizeof(struct sockaddr_in);
    while ((newSocket = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                                 (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c)) > 0)
    {
        printf("\nConnection accepted\n");

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
        struct Package *package = &buffer;

        // wrong connection stablishment package
        if (package->size < 1 || package->size > 16 || package->type != CONNECTION_TYPE)
        {
            printf("Wrong package size or type.\n");
            printf("size: %d   type %d\n", package->size, package->type);
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
            serverStopAndWait(connectionData->fileSize, serverInfo, package->destiny, package->size);
        else
            serverGoBackN(connectionData->fileSize, connectionData->windowSize, serverInfo, package->destiny, package->source);

        free(connectionData);

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
