#include "Client.hpp"
#include <iostream>
#include <thread>

Client::Client(){}

void Client::connect(const std::string& host, unsigned short port)
{
    socket_.connectTo(host, port);
    std::string welcome = socket_.receiveLine();
}

void Client::run()
{
    std::thread inputThread(&Client::inputLoop, this);
    std::thread outputThread(&Client::outputLoop, this);

    inputThread.join();
    outputThread.join();

}

void Client::inputLoop()
{
    std::string line;
    while (std::getline(std::cin, line))
    {
        try
        {
            socket_.sendAll(line + "\n");
        }
        catch(const std::exception& e)
        {
            std::cerr << "Erro no envio de dados: " << e.what() << std::endl;
            break;
        }
    }
}

void Client::outputLoop()
{
    while(1)
    {
        try
        {
            std::string received = socket_.receiveLine();
            std::cout << received << std::endl;
        }
    
        catch (const std::exception& e)
        {
            std::cerr << "Conexão encerrada: " << e.what() << std::endl;
            break;
    }
    }
}
