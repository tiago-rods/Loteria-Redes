#pragma once 

#include <winsock2.h>
#include <Windows.h>
#include <string>

class Socket {
public: 
    Socket(); // Cria um socket TCP
    explicit Socket(SOCKET); //explicit faz com que seja necessário o uso de um cast para o construtor
    ~Socket(); // Destrutor

    // Sockets não podem ser copiados, logo é necessário utilizar move semantics
    Socket(const Socket&) = delete; // Desabilita o construtor de cópia
    Socket& operator = (const Socket&) = delete; // Desabilita o operador de atribuição de cópia

    Socket(Socket&&) noexcept; // Habilita o construtor de movimento
    Socket& operator = (Socket&&) noexcept; // Habilita o operador de atribuição de movimento

    void connectTo(const std::string& host, unsigned short port); // Conecta o socket a um host e porta
    void bindTo(unsigned short port); // Associa o socket a uma porta
    void listenFor(int backlog = SOMAXCONN); // coloca o socket em modo de escuta
    Socket accept();

    void sendAll(const std::string& data);
    std::string receiveLine(); //Lê até encontrar um /n 

    void close();
    bool isValid() const; // Verifica se o socket é válido obs: const no final do método indica que ele não modifica o estado do objeto

private:
    SOCKET handle_; // Handle do socket
};