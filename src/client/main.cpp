#include"Client.hpp"
#include"../common/WinsockGuard.hpp"
#include<iostream>

//vou deixar porta fixa por enquanto, podemos mudar para deixar porta por argv posteriormente
int main(int argc, char* argv[])
{
    std::string host = "127.0.0.1"; // host e port padrão, caso não queira passar por argv
    unsigned short port = 54000;

    //para caso decidamos passar por argc

    if(argc >= 2 ) host = argv[1]; 
    if (argc >= 3 && atoi(argv[2]) <= 65535) port = static_cast<unsigned short>(std::atoi(argv[2])); // portas TCP vão até 65535

    try
    {
        WinsockGuard guard; // inicia RAII do Socket

        Client client; // cria objeto client
        client.connect(host, port); // tenta a conexão na porta e no host declarados
        client.run(); // tenta rodar a conexão
    }
    catch(const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << std::endl; // caso não funcione, explica o motivo e retorna 1
        return 1;
    }
    return 0;
}