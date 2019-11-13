#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <fstream>

#include "IBaseService.h"

#define SERVER_PORT 8888
#define SERVER_IP "127.0.0.1"
#define MAX_BUFFER_SIZE 1024

class ClientService : public IBaseService
{
public:
    ClientService(IFlowControlUnit flowControl);
    ~ClientService();
    void Start();
};