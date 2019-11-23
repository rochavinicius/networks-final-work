#ifndef __MACROS_H__
#define __MACROS_H__

#include <sys/types.h>
#include <netinet/in.h>

#define SERVER_DEFAULT_IP "127.0.0.1"
#define SERVER_DEFAULT_PORT 8888
#define MAX_BUFFER_SIZE 256

#define DATA_TYPE 0
#define ACK_TYPE 1
#define CONNECTION_TYPE 2

// polynome: x⁸ + x⁶ + x³ + x + 1 ->  1 0100 1011 (bin), 0x14B (hexa), 331 (dec)
#define CRC_POLYNOME 331

struct Package
{
    uint32_t destiny;
    uint32_t source;
    uint8_t type;
    uint8_t sequency;
    uint16_t size;
    uint32_t crc;
    unsigned char data[240];
} __attribute__((__packed__));

struct ConnectionData
{
    uint32_t flowControl;
    uint8_t windowSize;
    uint32_t fileSize;
} __attribute__((__packed__));

struct ServerInfo
{
    int socket;
    struct sockaddr_in clientaddr;
    struct sockaddr_in sockaddr;
};

struct ClientInfo
{
    int socket;
    struct sockaddr_in sockaddr;
    char *clientIp;
};

#endif