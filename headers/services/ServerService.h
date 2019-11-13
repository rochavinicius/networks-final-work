#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <iostream>
#include <vector>
#include <pthread.h>
#include <string.h>

#include "IBaseService.h"
#include "../models/Client.h"

#define SERVER_PORT 8888
#define MAX_BUFFER_SIZE 255

class ServerService : public IBaseService
{
public:
    ServerService(IFlowControlUnit flowControl);
    ~ServerService();
    void Start();

private:
    // void HandlerThread(void *ptr);

    // std::vector<Client> m_clients;
};