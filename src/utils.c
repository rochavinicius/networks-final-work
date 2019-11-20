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

//TODO criar funcao pra converter a struct Package pra network byte, fazer campo a campo

struct ConnectionData *parseToConnectionData(char input[])
{
    struct ConnectionData *connectionData = (struct ConnectionData *)malloc(sizeof(struct ConnectionData));
    memcpy(connectionData, &input[0], 9);

    return connectionData;
}

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
