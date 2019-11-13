#include "../../headers/models/Client.h"

Client::Client(int n, int s)
    : m_number(n), m_socket(s)
{
}

Client::~Client()
{
}

int Client::GetSocket()
{
    return this->m_socket;
}

int Client::GetNumber()
{
    return this->m_number;
}