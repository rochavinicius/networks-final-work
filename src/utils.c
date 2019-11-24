#include "utils.h"

/*
             CRC32 computation
*******************************************************************************
* source:
* https://community.nxp.com/thread/330919
*
* Algorithm changed to generate table based on the following custom polynomial:
* 0x14b
*******************************************************************************
*/

bool tableCreated = false;
u_int32_t crcTable[256];

void make_crc_table()
{
    unsigned long POLYNOMIAL = CRC_POLYNOME;
    unsigned long remainder;
    unsigned char b = 0;
    do
    {
        // Start with the data byte
        remainder = b;
        for (unsigned long bit = 8; bit > 0; --bit)
        {
            if (remainder & 1)
                remainder = (remainder >> 1) ^ POLYNOMIAL;
            else
                remainder = (remainder >> 1);
        }
        crcTable[(size_t)b] = remainder;
    } while (0 != ++b);
}

u_int32_t Crc32_ComputeBuf(u_int32_t crc32, const void *buf, int buflen)
{
    if (!tableCreated)
    {
        make_crc_table();
        tableCreated = true;
    }
    unsigned char *pbuf = (unsigned char *)buf;

    int i;
    int iLookup;

    for (i = 0; i < buflen; i++)
    {
        iLookup = (crc32 & 0xFF) ^ (*pbuf++);
        crc32 = ((crc32 & 0xFFFFFF00) >> 8) & 0xFFFFFF; // ' nasty shr 8 with vb :/
        crc32 = crc32 ^ crcTable[iLookup];
    }

    return crc32;
}

u_int32_t crc32_of_buffer(const char *buf, int buflen)
{
    return Crc32_ComputeBuf(0xFFFFFFFF, buf, buflen) ^ 0xFFFFFFFF;
}

/*
*******************************************************************************
*/

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

void clientStopAndWait(int fileSize, char fileName[], struct ClientInfo *clientInfo, bool hasErrorInsertion)
{
    FILE *file = fopen(fileName, "r");
    char dataBuffer[240];
    int nrBytesRead;
    struct Package package;
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
        printf("Error insertion mask: %d \n", errorInsertionMask);
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
        package.crc = crc32_of_buffer((const char *)&package, sizeof(struct Package));

        // if has error insertion module, then flip size bits
        if (hasErrorInsertion)
        {
            package.size &= errorInsertionMask;
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

        if (buffer.type != ACK_TYPE)
        {
            perror("Server response not acknoledge Stop and Wait.\n");
            exit(1);
        }

        incrementedSequency = sequency + 1;
        if (incrementedSequency > 1)
            incrementedSequency = 0;

        printf("ACK SEQUENCY %d\n", buffer.sequency);
        printf("INCREMENTED SEQUENCY %d\n", incrementedSequency);

        while (buffer.sequency != incrementedSequency)
        {
            printf("Sending frame retransmission to server.\n");

            // Set data, size, sequency and CRC
            memcpy(package.data, dataBuffer, nrBytesRead);
            package.size = nrBytesRead;
            package.sequency = sequency;
            package.crc = 0;
            package.crc = crc32_of_buffer((const char *)&package, sizeof(struct Package));

            printf("Sending retransmission CRC: %d\n", package.crc);

            parsePackageToNetwork(&package);

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

            if (buffer.type != ACK_TYPE)
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
/*void clientGoBackN(int fileSize, char fileName[], struct ClientInfo *clientInfo, int windowSize, bool hasErrorInsertion)
{
    FILE *file = fopen(fileName, "r");

    char dataBuffer[240];
    int nrBytesRead;
    struct Package buffer;
    struct Package *frames = (struct Package *)malloc(windowSize * sizeof(struct Package));
    int retSend;
    int retRecv;
    int sequency = 0;
    int incrementedSequency;
    int c = sizeof(struct sockaddr_in);
    int i;
    int j;
    int k;
    int nrFramesSent;
    uint16_t correctSize;
    bool backToBegin = false;

    printf("\n\nStarting Go Back N protocol...\n");
    printf("File size: %d\n", fileSize);

    // error insertion module
    int errorInsertionMask;
    int frameWithError;
    if (hasErrorInsertion)
    {
        srand(5);
        errorInsertionMask = rand() % 256;
        frameWithError = 1;
        printf("ERROR MASK: %d, frame to have bitflip: %d\n", errorInsertionMask, frameWithError);
    }

    while (fileSize > 0)
    {
        nrFramesSent = 0;
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
            frames[sequency].crc = crc32_of_buffer((const char *)&frames[sequency], sizeof(struct Package));

            printf("CRC calculated: %d\n", frames[sequency].crc);

            printf("Sending package to %d server.\n", sequency);
            printf("Sending size %d\n", frames[sequency].size);

            parsePackageToNetwork(&frames[sequency]);

            printf("File size: %d\n", fileSize);

            if (hasErrorInsertion && sequency == frameWithError)
            {
                printf("\n\nApplying error mask\n\n");
                correctSize = nrBytesRead;
                frames[sequency].size ^= errorInsertionMask;
            }

            // Send package to server
            retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                             0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
            if (retSend < 0)
            {
                perror("Error sending package in Stop and Wait.\n");
                exit(1);
            }

            nrFramesSent++;

            sequency++;
            if (sequency == windowSize)
                sequency = 0;
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

        // basic validation for server response: it must be an ack
        if (buffer.type != ACK_TYPE)
        {
            perror("Server response not acknoledge Stop and Wait.\n");
            exit(1);
        }

        printf("ACK SEQUENCY %d\n", buffer.sequency);

        incrementedSequency = sequency;
        for (j = 0; j < nrFramesSent; j++)
        {
            incrementedSequency++;
            if (incrementedSequency == windowSize)
            {
                incrementedSequency = 0;
            }
        }

        printf("INCREMENTED SEQUENCY %d\n", incrementedSequency);

        if (incrementedSequency != buffer.sequency)
        {
            // decrement file size sice some of them were discarted from server
            while (sequency != buffer.sequency)
            {
                if (nrBytesRead != 240)
                {
                    fileSize += nrBytesRead;
                    nrBytesRead = 240;
                }
                else
                {
                    fileSize += nrBytesRead;
                }

                sequency--;
                if (sequency < 0)
                    sequency = windowSize - 1;
            }
        }

        // server requesting frame retransmission
        // send all the frames that were dropped from server
        while (incrementedSequency != buffer.sequency)
        {
            sequency = buffer.sequency;
            nrFramesSent = 0;
            printf("Sending frame retransmission to server.\n");

            for (j = 0; j < windowSize; j++)
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
                    frames[sequency].crc = crc32_of_buffer((const char *)&frames[sequency], sizeof(struct Package));

                    parsePackageToNetwork(&frames[sequency]);

                    retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                                     0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
                    if (retSend <= 0)
                    {
                        perror("Error sending package retransmission in Stop and Wait.\n");
                        exit(1);
                    }
                    nrFramesSent++;
                }
                else
                {
                    if (hasErrorInsertion && sequency == frameWithError)
                    {
                        frames[sequency].size = correctSize;
                        frames[sequency].crc = 0;
                        frames[sequency].crc = crc32_of_buffer((const char *)&frames[sequency], sizeof(struct Package));
                    }

                    fileSize -= nrBytesRead;

                    // Do data retransmission
                    retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                                     0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
                    if (retSend <= 0)
                    {
                        perror("Error sending package retransmission in Stop and Wait.\n");
                        exit(1);
                    }
                    nrFramesSent++;
                }

                sequency++;
                if (sequency == windowSize)
                {
                    sequency = 0;
                    backToBegin = true;
                }

                // increment sequency for window size frames
                // for (k = 0; k < windowSize; k++)
                // {
                //     sequency++;
                //     if (sequency == windowSize)
                //     {
                //         sequency = 0;
                //         backToBegin = true;
                //     }
                // }
            }

            incrementedSequency = sequency;
            for (j = 0; j < nrFramesSent; j++)
            {
                incrementedSequency++;
                if (incrementedSequency == windowSize)
                {
                    incrementedSequency = 0;
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

            if (buffer.type != ACK_TYPE)
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
}*/

void clientGoBackN(int fileSize, char fileName[], struct ClientInfo *clientInfo, int windowSize, bool hasErrorInsertion)
{
    FILE *file = fopen(fileName, "r");

    char dataBuffer[240];
    int nrBytesRead;
    struct Package buffer;
    struct Package *frames = (struct Package *)malloc(windowSize * sizeof(struct Package));
    int retSend;
    int retRecv;
    int sequency = 0;
    int incrementedSequency;
    int c = sizeof(struct sockaddr_in);
    int i;
    int j;
    int k;
    int nrFramesSent;
    uint16_t correctSize;
    bool backToBegin = false;

    printf("\n\nStarting Go Back N protocol...\n");
    printf("File size: %d\n", fileSize);

    // error insertion module
    int errorInsertionMask;
    int frameWithError;
    if (hasErrorInsertion)
    {
        srand(5);
        errorInsertionMask = rand() % 256;
        frameWithError = 1;
        printf("ERROR MASK: %d, frame to have bitflip: %d\n", errorInsertionMask, frameWithError);
    }

    while (fileSize > 0)
    {
        nrFramesSent = 0;
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
            frames[sequency].crc = crc32_of_buffer((const char *)&frames[sequency], sizeof(struct Package));

            printf("CRC calculated: %d\n", frames[sequency].crc);

            printf("Sending package to %d server.\n", sequency);
            printf("Sending size %d\n", frames[sequency].size);

            parsePackageToNetwork(&frames[sequency]);

            if (hasErrorInsertion && sequency == frameWithError)
            {
                printf("\nApplying error mask\n\n");
                // store correctSize for that frame
                correctSize = nrBytesRead;
                frames[sequency].size ^= errorInsertionMask;
            }

            // Send package to server
            retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                             0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
            if (retSend < 0)
            {
                perror("Error sending package in Go Back N.\n");
                exit(1);
            }

            nrFramesSent++;

            sequency++;
            if (sequency == windowSize)
                sequency = 0;

            fileSize -= nrBytesRead;
            printf("File size: %d\n", fileSize);
        }

        // waits for ack form the server
        retRecv = recvfrom(clientInfo->socket, (char *)&buffer, sizeof(buffer),
                           0, (struct sockaddr *)&clientInfo->sockaddr,
                           &c);
        if (retRecv < 0)
        {
            perror("Error recvfrom <= 0 Go Back N.\n");
            exit(1);
        }

        parseNetworkToPackage(&buffer);

        // basic validation for server response: it must be an ack
        if (buffer.type != ACK_TYPE)
        {
            perror("Server response not acknoledge Go Back N.\n");
            exit(1);
        }
        printf("ACK SEQUENCY %d\n", buffer.sequency);

        // calculate expected return sequency for happy path
        printf("SEQUENCY DEBUGINDSIAOD %d\n\n", sequency);
        incrementedSequency = sequency;
        // if (nrFramesSent != windowSize)
        // {
        //     incrementedSequency++;
        // }
        // else
        // {
        //     for (j = 0; j < nrFramesSent; j++)
        //     {
        //         incrementedSequency++;
        //         if (incrementedSequency == windowSize)
        //         {
        //             incrementedSequency = 0;
        //         }
        //     }
        // }
        printf("INCREMENTED SEQUENCY %d\n", incrementedSequency);

        /*
        *   This part handles frame retransmission  *********
        */
        while (incrementedSequency != buffer.sequency)
        {
            /*
            * Decrement file size from count since it has been dropped
            */
            // decrement file size sice some of them were discarted from server
            while (sequency != buffer.sequency)
            {
                if (nrBytesRead != 240)
                {
                    fileSize += nrBytesRead;
                    nrBytesRead = 240;
                }
                else
                {
                    fileSize += nrBytesRead;
                }

                sequency--;
                if (sequency < 0)
                    sequency = windowSize - 1;
            }
            /*
            * End of decrementing
            */

            sequency = buffer.sequency;
            nrFramesSent = 0;
            printf("Sending frames retransmission to server.\n");

            // printf("NAO EH PARA CAIR AKI\n");
            for (i = 0; i < windowSize; i++)
            {
                if (fileSize <= 0)
                    break;

                if (!backToBegin)
                {
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
                    frames[sequency].crc = crc32_of_buffer((const char *)&frames[sequency], sizeof(struct Package));

                    printf("CRC calculated: %d\n", frames[sequency].crc);

                    printf("Sending package to %d server.\n", sequency);
                    printf("Sending size %d\n", frames[sequency].size);

                    parsePackageToNetwork(&frames[sequency]);

                    printf("File size: %d\n", fileSize);

                    // if (hasErrorInsertion && sequency == frameWithError)
                    // {
                    //     printf("\nApplying error mask\n\n");
                    //     // store correctSize for that frame
                    //     correctSize = nrBytesRead;
                    //     frames[sequency].size ^= errorInsertionMask;
                    // }

                    // Send package to server
                    retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                                     0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
                    if (retSend < 0)
                    {
                        perror("Error sending package in Go Back N.\n");
                        exit(1);
                    }
                    nrFramesSent++;
                    fileSize -= nrBytesRead;
                }
                else
                {
                    if (hasErrorInsertion && sequency == frameWithError)
                    {
                        frames[sequency].size = correctSize;
                        frames[sequency].crc = 0;
                        frames[sequency].crc = crc32_of_buffer((const char *)&frames[sequency], sizeof(struct Package));
                    }

                    fileSize -= frames[sequency].size;

                    // Do data retransmission
                    retSend = sendto(clientInfo->socket, (const void *)&frames[sequency], sizeof(struct Package),
                                     0, (const struct sockaddr *)&clientInfo->sockaddr, sizeof(struct sockaddr));
                    if (retSend <= 0)
                    {
                        perror("Error sending package retransmission in Stop and Wait.\n");
                        exit(1);
                    }
                    nrFramesSent++;
                }

                sequency++;
                if (sequency == windowSize)
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
            if (buffer.type != ACK_TYPE)
            {
                perror("Server response not acknoledge Stop and Wait.\n");
                exit(1);
            }

            // calculate expected return sequency for happy path
            incrementedSequency = sequency;
            for (j = 0; j < nrFramesSent; j++)
            {
                incrementedSequency++;
                if (incrementedSequency == windowSize)
                {
                    incrementedSequency = 0;
                }
            }
            printf("INCREMENTED SEQUENCY %d\n", incrementedSequency);
        }
        /*
        *   End of retransmission handling          *********
        */

        sequency = incrementedSequency;
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

        // check for CRC
        unsigned int receivedCrc = buffer.crc;
        buffer.crc = 0;
        crc = crc32_of_buffer((const char *)&buffer, sizeof(struct Package));

        printf("Received CRC: %d, Calculated CRC: %d \n", receivedCrc, crc);

        // bitflip, ask for retransmission
        while (crc != receivedCrc)
        {
            package.destiny = destiny;
            package.source = source;
            package.type = ACK_TYPE;
            package.sequency = sequency;

            parsePackageToNetwork(&package);

            printf("Package has bitflips, requesting retransmission.\n");

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

            // check for CRC
            receivedCrc = buffer.crc;
            buffer.crc = 0;
            crc = crc32_of_buffer((const char *)&buffer, sizeof(struct Package));

            printf("Received CRC: %d, Calculated CRC: %d \n", receivedCrc, crc);
        }

        // write to file
        fwrite(buffer.data, sizeof(buffer.data[0]),
               buffer.size, file);

        // decrement file size
        fileSize -= buffer.size;

        printf("File size: %d, size received %d\n", fileSize, buffer.size);

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

/*void serverGoBackN(int fileSize, int windowSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source)
{
    FILE *file;

    struct Package buffer;
    struct Package package;
    int retSend;
    int retRecv;
    int sequency = 0;
    unsigned int crc;
    int c = sizeof(struct sockaddr_in);
    int i;
    unsigned int receivedCrc;

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
        // for (i = 0; i < windowSize; i++)
        // {
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

        printf("Received package from client.\n");

        // check for CRC
        receivedCrc = buffer.crc;
        buffer.crc = 0;
        crc = crc32_of_buffer((const char *)&buffer, sizeof(struct Package));

        printf("Received CRC: %d, Calculated CRC: %d \n", receivedCrc, crc);

        printf("Received sequency %d\n", sequency);

        // package has bitflips
        if (crc != receivedCrc)
        {
            for (int j = 0; j < 5; j++)
            {
                // discard following packages
                retRecv = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                                   (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c);
                if (retRecv < 0)
                {
                    perror("Error recvfrom <= 0 Go Back N.\n");
                    exit(1);
                }
            }

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
            // break;
        }
        else
        {
            // No bit flip in frame

            // write to file
            fwrite(buffer.data, sizeof(buffer.data[0]),
                   buffer.size, file);

            // decrement file size
            fileSize -= buffer.size;

            printf("File size: %d, size received %d\n", fileSize, buffer.size);

            // for (int j = 0; j < windowSize; j++)
            // {
            sequency++;
            if (sequency == windowSize)
                sequency = 0;
            // }
            printf("Next sequency %d\n", sequency);
            // }

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
    }

    // for (int j = 0; j < windowSize; j++)
    // {
    //     sequency++;
    //     if (sequency == windowSize)
    //         sequency = 0;
    // }
    sequency++;
    if (sequency == windowSize)
        sequency = 0;
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
}*/

void serverGoBackN(int fileSize, int windowSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source)
{
    FILE *file;

    struct Package buffer;
    struct Package package;
    int retSend;
    int retRecv;
    int sequency = 0;
    unsigned int crc;
    int c = sizeof(struct sockaddr_in);
    int i;
    unsigned int receivedCrc;
    bool corruptedPackage;

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
        corruptedPackage = false;
        for (i = 0; i < windowSize; i++)
        {
            if (fileSize <= 0)
                break;

            if (corruptedPackage)
            {
                // in this case just drop the following packages
                retRecv = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                                   (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c);
                if (retRecv < 0)
                {
                    perror("Error recvfrom <= 0 Go Back N.\n");
                    exit(1);
                }
            }
            else
            {

                // receive package
                retRecv = recvfrom(serverInfo->socket, (char *)&buffer, sizeof(buffer), 0,
                                   (struct sockaddr *)&serverInfo->clientaddr, (socklen_t *)&c);
                if (retRecv < 0)
                {
                    perror("Error recvfrom <= 0 Go Back N.\n");
                    exit(1);
                }

                parseNetworkToPackage(&buffer);

                printf("Received package from client.\n");

                // check for CRC
                receivedCrc = buffer.crc;
                buffer.crc = 0;
                crc = crc32_of_buffer((const char *)&buffer, sizeof(struct Package));

                printf("Received CRC: %d, Calculated CRC: %d \n", receivedCrc, crc);
                printf("Received sequency %d\n", buffer.sequency);

                // package has bitflips
                if (crc != receivedCrc)
                {
                    corruptedPackage = true;
                    continue;
                }

                // write to file
                fwrite(buffer.data, sizeof(buffer.data[0]),
                       buffer.size, file);

                // decrement file size
                fileSize -= buffer.size;

                printf("File size: %d, size received %d\n", fileSize, buffer.size);

                sequency++;
                if (sequency == windowSize)
                    sequency = 0;
                printf("Next sequency %d\n", sequency);
            }
        }

        // request next frames or retransmission
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

    sequency++;
    if (sequency == windowSize)
        sequency = 0;
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