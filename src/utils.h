#ifndef __UTILS_H__
#define __UTILS_H__

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

unsigned int crc32b(unsigned char *message, unsigned int polynome);

void parsePackageToNetwork(struct Package *package);
void parseNetworkToPackage(struct Package *package);

struct ConnectionData *parseToConnectionData(char input[9]);

void clientStopAndWait(int fileSize, char fileName[], struct ClientInfo *clientInfo, bool hasErrorInsertion);
void clientGoBackN(int fileSize, char fileName[], struct ClientInfo *clientInfo, int windowSize, bool hasErrorInsertion);

void serverStopAndWait(int fileSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source);
void serverGoBackN(int fileSize, int windowSize, struct ServerInfo *serverInfo, uint32_t destiny, uint32_t source);

#endif