#include "utils.h"

// CRC algorithm used from source :
// https://stackoverflow.com/questions/21001659/crc32-algorithm-implementation-in-c-without-a-look-up-table-and-with-a-public-li
unsigned int crc32b(unsigned char *message, unsigned int polynome)
{
    int i, j;
    unsigned int byte, crc, mask;

    // mask = polynome;

    i = 0;
    crc = 0xFFFFFFFF;
    // crc = 0x0;
    while (message[i] != 0)
    {
        byte = message[i]; // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--)
        { // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (0xEDB88320 & mask);
        }
        i = i + 1;
    }
    return crc;
}

void parsePackageToNetwork(struct Package *package)
{
    package->destiny = htonl(package->destiny);
    package->source = htonl(package->source);
    package->size = htons(package->size);
    package->crc = htonl(package->crc);
}

void parseNetworkToPackage(struct Package *package)
{
    package->destiny = ntohl(package->destiny);
    package->source = htonl(package->source);
    package->size = ntohs(package->size);
    package->crc = ntohl(package->crc);
}

struct ConnectionData *parseToConnectionData(char input[])
{
    struct ConnectionData *connectionData = (struct ConnectionData *)malloc(sizeof(struct ConnectionData));
    memcpy(connectionData, &input[0], 9);

    return connectionData;
}

// Flow Control Methods

// CLIENT

//TODO verificar se a mascara de erro funciona e replicar para o go back N
void clientStopAndWait(int fileSize, char fileName[], struct ClientInfo *clientInfo, bool hasErrorInsertion)
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

    // error insertion module
    int errorInsertionMask;
    if (hasErrorInsertion)
    {
        srand(5);
        errorInsertionMask = rand() % 256;
        printf("ERROR MASK: %d \n", errorInsertionMask);
    }

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

        // if has error insertion module, then flip size bits
        if (hasErrorInsertion)
        {
            package.size ^= errorInsertionMask;
        }

        printf("Sending package to server.\n");
        printf("Sending size %d\n", package.size);

        parsePackageToNetwork(&package);

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

        incrementedSequency = sequency + 1;
        if (incrementedSequency > 1)
            incrementedSequency = 0;

        printf("ACK SEQUENCY %d\n", ackPackage->sequency);
        printf("INCREMENTED SEQUENCY %d\n", incrementedSequency);

        while (ackPackage->sequency != incrementedSequency)
        {
            printf("Sending frame retransmission to server.\n");

            // Set data, size, sequency and CRC
            memcpy(package.data, dataBuffer, nrBytesRead);
            package.size = nrBytesRead;
            package.sequency = sequency;
            package.crc = 0;
            package.crc = crc32b((unsigned char *)&package, CRC_POLYNOME);

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
void clientGoBackN(int fileSize, char fileName[], struct ClientInfo *clientInfo, int windowSize, bool hasErrorInsertion)
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

    printf("\n\nStarting Go Back N protocol...\n");
    printf("File size: %d\n", fileSize);

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
            printf("Read %d bytes from file.\n", nrBytesRead);

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

            printf("Sending package to server.\n");
            printf("Sending size %d\n", frames[sequency].size);

            parsePackageToNetwork(&frames[sequency]);

            printf("File size: %d\n", fileSize);

            // Send package to server
            retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                             0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
            if (retSend < 0)
            {
                perror("Error sending package in Stop and Wait.\n");
                exit(1);
            }

            sequency = sequency + 1 == windowSize ? 0 : sequency++;
            fileSize -= nrBytesRead;
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

        incrementedSequency = sequency;
        for (int j = 0; j < windowSize; j++)
        {
            incrementedSequency++;
            if (incrementedSequency == windowSize)
            {
                incrementedSequency = 0;
            }
        }

        printf("INCREMENTED SEQUENCY %d\n", incrementedSequency);

        // server requesting frame retransmission
        // send all the frames that were dropped from server
        while (incrementedSequency != ackPackage->sequency)
        {
            //TODO em caso de retransmissao tem q decrementar o sizeToDecrement, pq dai enviou menos do arquivo
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

                for (int j = 0; j < windowSize; j++)
                {
                    sequency++;
                    if (sequency == windowSize)
                    {
                        sequency = 0;
                        backToBegin = true;
                    }
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

// SERVER

void serverStopAndWait(int fileSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source)
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

    printf("\n\nStarting Stop and Wait protocol...\n");

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

        receivedPackage = &buffer;

        // check for CRC
        crc = crc32b((unsigned char *)receivedPackage, CRC_POLYNOME);

        printf("Received CRC: %d, Calculated CRC: %d \n", receivedPackage->crc, crc);

        // bitflip, ask for retransmission
        while (crc != buffer.crc)
        {
            package.destiny = destiny;
            package.source = source;
            package.type = ACK_TYPE;
            package.sequency = sequency;

            parsePackageToNetwork(&package);

            printf("Package has bitflps, requesting retransmission.\n");

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

            receivedPackage = &buffer;

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

void serverGoBackN(int fileSize, int windowSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source)
{
    FILE *file;

    struct Package buffer;
    struct Package package;
    struct Package *receivedPackage;
    int retSend;
    int retRecv;
    int sequency = 0;
    unsigned int crc;
    int c = sizeof(struct sockaddr_in);
    int i;

    printf("\n\nStarting Go Back N protocol...\n");

    file = fopen("receivedFile.txt", "w");
    if (file == NULL)
    {
        perror("Error while creating received file.\n");
        exit(1);
    }

    printf("File size initial: %d, window size: %d\n", fileSize, windowSize);

    while (fileSize > 0)
    {
        for (i = 0; i < windowSize; i++)
        {
            if (fileSize <= 0)
                break;

            // receive package
            retRecv = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                               (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c);
            if (retRecv < 0)
            {
                perror("Error recvfrom <= 0 Go Back N.\n");
                exit(1);
            }

            parseNetworkToPackage(&buffer);
            receivedPackage = &buffer;

            printf("Received package from client.\n");

            // check for CRC
            crc = crc32b((unsigned char *)receivedPackage, CRC_POLYNOME);

            printf("Received CRC: %d, Calculated CRC: %d \n", receivedPackage->crc, crc);

            printf("Received sequency %d\n", receivedPackage->sequency);

            // package has bitflips
            if (crc != receivedPackage->crc)
            {
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
                    perror("Error sending package to client. Go Back N\n");
                    exit(1);
                }

                printf("Retransmission of frame %d requested.\n", sequency);
                break;
            }

            // No bit flip in package

            // write to file
            fwrite(receivedPackage->data, sizeof(receivedPackage->data[0]),
                   receivedPackage->size, file);

            // decrement file size
            fileSize -= receivedPackage->size;

            printf("File size: %d, size received %d\n", fileSize, receivedPackage->size);

            for (int j = 0; j < windowSize; j++)
            {
                sequency++;
                if (sequency == windowSize)
                    sequency = 0;
            }
            printf("Next sequency %d\n", sequency);
        }

        // request next frames
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
    }

    for (int j = 0; j < windowSize; j++)
    {
        sequency++;
        if (sequency == windowSize)
            sequency = 0;
    }
    printf("Next sequency %d\n", sequency);

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