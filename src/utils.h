#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <string.h>
#include "macros.h"

unsigned int crc32b(unsigned char *message, unsigned int polynome);

struct Package *parseToPackage(struct Package *networkPackage);
struct ConnectionData *parseToConnectionData(char input[9]);

#endif