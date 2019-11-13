#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <netdb.h>

#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"
#define MAX_BUFFER_SIZE 1024
#define DATA_TYPE 0
#define ACK_TYPE 1
#define CONNECTION_TYPE 2

// polinomio: 1000 1011 (bin), 0x8C (hexa), 139 (dec)
#define CRC_POLYNOME 139

unsigned char const crc8x_table[] = {
    0x00, 0x31, 0x62, 0x53, 0xc4, 0xf5, 0xa6, 0x97, 0xb9, 0x88, 0xdb, 0xea, 0x7d,
    0x4c, 0x1f, 0x2e, 0x43, 0x72, 0x21, 0x10, 0x87, 0xb6, 0xe5, 0xd4, 0xfa, 0xcb,
    0x98, 0xa9, 0x3e, 0x0f, 0x5c, 0x6d, 0x86, 0xb7, 0xe4, 0xd5, 0x42, 0x73, 0x20,
    0x11, 0x3f, 0x0e, 0x5d, 0x6c, 0xfb, 0xca, 0x99, 0xa8, 0xc5, 0xf4, 0xa7, 0x96,
    0x01, 0x30, 0x63, 0x52, 0x7c, 0x4d, 0x1e, 0x2f, 0xb8, 0x89, 0xda, 0xeb, 0x3d,
    0x0c, 0x5f, 0x6e, 0xf9, 0xc8, 0x9b, 0xaa, 0x84, 0xb5, 0xe6, 0xd7, 0x40, 0x71,
    0x22, 0x13, 0x7e, 0x4f, 0x1c, 0x2d, 0xba, 0x8b, 0xd8, 0xe9, 0xc7, 0xf6, 0xa5,
    0x94, 0x03, 0x32, 0x61, 0x50, 0xbb, 0x8a, 0xd9, 0xe8, 0x7f, 0x4e, 0x1d, 0x2c,
    0x02, 0x33, 0x60, 0x51, 0xc6, 0xf7, 0xa4, 0x95, 0xf8, 0xc9, 0x9a, 0xab, 0x3c,
    0x0d, 0x5e, 0x6f, 0x41, 0x70, 0x23, 0x12, 0x85, 0xb4, 0xe7, 0xd6, 0x7a, 0x4b,
    0x18, 0x29, 0xbe, 0x8f, 0xdc, 0xed, 0xc3, 0xf2, 0xa1, 0x90, 0x07, 0x36, 0x65,
    0x54, 0x39, 0x08, 0x5b, 0x6a, 0xfd, 0xcc, 0x9f, 0xae, 0x80, 0xb1, 0xe2, 0xd3,
    0x44, 0x75, 0x26, 0x17, 0xfc, 0xcd, 0x9e, 0xaf, 0x38, 0x09, 0x5a, 0x6b, 0x45,
    0x74, 0x27, 0x16, 0x81, 0xb0, 0xe3, 0xd2, 0xbf, 0x8e, 0xdd, 0xec, 0x7b, 0x4a,
    0x19, 0x28, 0x06, 0x37, 0x64, 0x55, 0xc2, 0xf3, 0xa0, 0x91, 0x47, 0x76, 0x25,
    0x14, 0x83, 0xb2, 0xe1, 0xd0, 0xfe, 0xcf, 0x9c, 0xad, 0x3a, 0x0b, 0x58, 0x69,
    0x04, 0x35, 0x66, 0x57, 0xc0, 0xf1, 0xa2, 0x93, 0xbd, 0x8c, 0xdf, 0xee, 0x79,
    0x48, 0x1b, 0x2a, 0xc1, 0xf0, 0xa3, 0x92, 0x05, 0x34, 0x67, 0x56, 0x78, 0x49,
    0x1a, 0x2b, 0xbc, 0x8d, 0xde, 0xef, 0x82, 0xb3, 0xe0, 0xd1, 0x46, 0x77, 0x24,
    0x15, 0x3b, 0x0a, 0x59, 0x68, 0xff, 0xce, 0x9d, 0xac};

unsigned crc8x_fast(unsigned crc, void const *mem, size_t len)
{
    unsigned char const *data = mem;
    if (data == NULL)
        return 0xff;
    crc &= 0xff;
    while (len--)
        crc = crc8x_table[crc ^ *data++];
    return crc;
}

struct Package
{
    char destiny[5];
    char source[5];
    uint8_t type;
    uint8_t sequency;
    uint16_t size;
    uint32_t crc;
    char data[240];
} __attribute__((__packed__));

struct ConnectionData
{
    uint32_t flowControl;
    uint8_t windowSize;
    uint32_t fileSize;
} __attribute__((__packed__));

struct Package parseToPackage(char input[255])
{
    struct Package package;
    char aux[5];

    // Destiny
    memcpy(package.destiny, &input[0], 4);

    // Source
    memcpy(package.source, &input[4], 4);

    // Type
    memcpy(aux, &input[8], 1);
    aux[4] = '\0';
    package.type = atoi(aux);

    // Sequency
    memcpy(aux, &input[9], 1);
    aux[4] = '\0';
    package.sequency = atoi(aux);

    // Size
    memcpy(aux, &input[10], 2);
    aux[4] = '\0';
    package.size = atoi(aux);

    // CRC
    memcpy(aux, &input[12], 4);
    aux[4] = '\0';
    package.crc = atoi(aux);

    // Data
    memcpy(package.data, &input[16], 240);

    return package;
}

struct ClientInfo
{
    int socket;
    struct sockaddr_in sockaddr;
    char *clientIp;
};

struct ClientInfo startClient()
{
    struct ClientInfo clientInfo;
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

    clientInfo.socket = clientSocket;
    clientInfo.sockaddr = server_addr;

    hostName = gethostname(hostBuffer, sizeof(hostBuffer));
    hostEntry = gethostbyname(hostBuffer);
    clientInfo.clientIp = inet_ntoa(*(struct in_addr *)hostEntry->h_addr_list[0]);

    return clientInfo;
}

// gcc client.c -o client
// ./client
void main()
{
    int newSocket;
    char buffer[MAX_BUFFER_SIZE];

    //TODO obter as informacoes por parametro, como windowSize e qual o controle de fluxo

    struct ClientInfo clientInfo = startClient();

    while (1)
    {
        FILE *file;
        char fileName[256];
        bool isValidFile = false;
        char inputData[MAX_BUFFER_SIZE];
        ssize_t retSend;
        ssize_t retRecv;

        // get input file that will be sent to server
        while (!isValidFile)
        {
            printf("Enter file name or path to send to server:");
            printf("Type 'exit' to exit program.");

            fgets(fileName, sizeof(fileName), stdin);

            if (strcmp(fileName, "exit") == 0)
                exit(0);

            file = fopen(fileName, "r");

            if (file != NULL)
                isValidFile = true;
        }

        // send package to stablish connection with server

        struct ConnectionData data;
        // TODO obter esses parametros para mandar para o servidor
        // data.fileSize = fileSize;
        // data.flowControl = flowControl;
        // data.windowSize = windowSize;

        int clientIpFormatted = inet_addr(clientInfo.clientIp);

        struct Package package;
        memcpy(package.destiny, &clientInfo.sockaddr.sin_addr.s_addr, 4);
        memcpy(package.source, (const void *)&clientIpFormatted, 4);
        package.type = CONNECTION_TYPE;
        package.sequency = 0;
        package.size = sizeof(struct ConnectionData);
        memcpy(package.data, &data, sizeof(struct ConnectionData));
        package.crc = crc8x_fast(CRC_POLYNOME, (const void *)&package, sizeof(struct Package));

        retSend = sendto(clientInfo.socket, (const void *)&package, sizeof(struct Package),
                         0, (const struct sockaddr *)&clientInfo.sockaddr, sizeof(struct sockaddr));

        if (retSend < 0)
        {
            perror("Error sending package to server.\n");
            exit(EXIT_FAILURE);
        }

        // get confirmation ack from server
        retRecv = recvfrom(clientInfo.socket, inputData, sizeof(inputData),
                           MSG_WAITALL, (struct sockaddr *)&clientInfo.sockaddr,
                           &clientInfo.socket);

        struct Package ackPackage = parseToPackage(inputData);

        if (ackPackage.type != ACK_TYPE)
        {
            perror("Server response not acknoledge.\n");
            continue;
        }

        //TODO fazer a comunicacao utilizando a tecnica de controle de fluxo
        // if (flowControl == 0)
        //     executeStopAndWait(fileSize, windowSize);
        // else
        //     executeGoBackN(fileSize, windowSize);


        fclose(file);
    }

    close(clientInfo.socket);

    printf("Client socket connection closed. Finishing...");
}