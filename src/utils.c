#include "utils.h"
#include <stdio.h>

// CRC algorithm used from source :
// https://stackoverflow.com/questions/21001659/crc32-algorithm-implementation-in-c-without-a-look-up-table-and-with-a-public-li
unsigned int crc32b(unsigned char *message, unsigned int polynome)
{
    int i, j;
    unsigned int byte, crc, mask;

    i = 0;
    // crc = 0xFFFFFFFF;
    crc = 0x0;
    while (message[i] != 0)
    {
        byte = message[i]; // Get next byte.
        crc = crc ^ byte;
        for (j = 7; j >= 0; j--)
        { // Do eight times.
            mask = -(crc & 1);
            crc = (crc >> 1) ^ (polynome & mask);
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

// TODO verificar se eh msm necessario alocar um novo ao inves de alterar o recebido
struct Package *parseToPackage(struct Package *networkPackage)
{
    struct Package *package = (struct Package *)malloc(sizeof(struct Package));

    package->destiny = networkPackage->destiny;
    package->source = networkPackage->source;
    package->type = networkPackage->type;
    package->sequency = networkPackage->sequency;
    package->size = networkPackage->size;
    package->crc = networkPackage->crc;
    memcpy(&package->data, &networkPackage->data, sizeof(package->data));

    return package;
}
