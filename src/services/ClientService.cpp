#include "../../headers/services/ClientService.h"

ClientService::ClientService(IFlowControlUnit flowControl)
    : IBaseService(flowControl)
{
}

ClientService::~ClientService()
{
}

void ClientService::Start()
{
    int clientSocket;
    int newSocket;
    char buffer[MAX_BUFFER_SIZE];
    struct sockaddr_in server_addr;

    std::cout << "Starting client service..." << std::endl;

    // Creating socket file descriptor
    if ((clientSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        std::cerr << "Client socket creation failed." << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Client socket created." << std::endl;

    // Filling server information
    server_addr.sin_family = AF_INET; // IPv4
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(SERVER_PORT);

    std::cout << "Client started. Ready to send data..." << std::endl;

    while (1)
    {
        std::ifstream file;
        ssize_t retSend;
        ssize_t retRec;
        int len;
        std::string fileName;
        std::string message = "Hello";
        bool isValidFile = false;

        while (!isValidFile)
        {
            std::cout << "Enter file name or path to send to server:" << std::endl;
            std::cout << "Type 'exit' to exit program." << std::endl;
            std::cout << "> ";
            std::cin >> fileName;

            if (fileName == "exit") break;

            file.open(fileName);
            if (file.good())
            {
                isValidFile = true;
            }
            file.close();
        }

        m_flowContrulUnit.SendFile(fileName, clientSocket, server_addr);

        //TODO: move this code to SendFile method
        /*retSend = sendto(clientSocket, message.c_str(), message.length(), MSG_CONFIRM,
                         (const struct sockaddr *)&server_addr,
                         sizeof(server_addr));
        if (retSend < 0)
        {
            std::cerr << "Error sending package to server." << std::endl;
            exit(EXIT_FAILURE);
        }

        retRec = recvfrom(clientSocket, (char *)buffer, MAX_BUFFER_SIZE,
                          MSG_WAITALL, (struct sockaddr *)&server_addr,
                          &len);

        if (retRec < 0)
        {
            std::cerr << "Error retrieving message from server." << std::endl;
            exit(EXIT_FAILURE);
        }

        buffer[retRec] = '\0';*/
    }

    close(clientSocket);

    std::cout << "Client socket connection closed. Finishing..." << std::endl;
}