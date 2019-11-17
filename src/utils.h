#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdlib.h>
#include <string.h>
#include "macros.h"

unsigned crc8x_fast(unsigned crc, void const *mem, size_t len);

struct Package *parseToPackage(char input[256]);
struct ConnectionData *parseToConnectionData(char input[9]);

#endif