#pragma once
#include<string>
#include "../common/Socket.hpp"

class Client
{
public:
    Client();

    // método de conexão ao servidor
    void connect(const std::string& host, unsigned short port);

    // método que dispara ambas as threads

    void run();

private:
    void inputLoop();
    void outputLoop();

    Socket socket_;
};
