#pragma once

class Client
{
public:
    Client(int n, int s);
    ~Client();

    int GetNumber();
    int GetSocket();

private:
    int m_number;
    int m_socket;
};