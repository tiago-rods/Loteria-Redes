#pragma once
#include <atomic> 
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
    // métodos de escrita e leitura das threads
    void inputLoop();
    void outputLoop();

    Socket socket_;
    std::atomic<bool> running_ {true}; // flag atomica simples que uma thread seta e a outra le sem precisar de mutex
};
