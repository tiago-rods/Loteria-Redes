#include "Client.hpp"
#include <iostream>
#include <thread>

/*=======================
O que é: Construtor
O que faz: Nada além do que o Socket() padrão já faz (cria o socket TCP),
chamado implicitamente na inicialização do membro socket_
========================*/
Client::Client(){}

/*=======================
O que é: Conecta ao servidor e trata a MSG1
O que faz: Estabelece a conexão TCP e recebe a mensagem de boas-vindas
Como faz: connectTo() bloqueia até a conexão ser estabelecida; receiveLine()
bloqueia até chegar a MSG1 (o server manda ela logo após o accept)
========================*/
void Client::connect(const std::string& host, unsigned short port)
{
    socket_.connectTo(host, port);
    std::string welcome = socket_.receiveLine();
    std::cout << welcome << std::endl;
}

/*=======================
O que é: Orquestra as duas threads do cliente
O que faz: Dispara thread 1 (input) e thread 2 (output) e espera as duas terminarem
Como faz: std::thread recebe um ponteiro pra método (&Client::inputLoop) e
o "this" como primeiro argumento, pra saber em qual objeto chamar o método
Possíveis dúvidas: join() bloqueia até a thread terminar; sem ele, o destrutor
de std::thread chamaria std::terminate se a thread ainda estivesse rodando
========================*/
void Client::run()
{
    std::thread inputThread(&Client::inputLoop, this);
    std::thread outputThread(&Client::outputLoop, this);

    inputThread.join();
    outputThread.join();
}

/*=======================
O que é: Thread 1 - lê do teclado e envia pelo socket
O que faz: Em loop infinito, lê uma linha digitada e manda pro servidor
Como faz: std::getline lê até o '\n' do teclado mas descarta esse '\n';
como Socket::receiveLine() do lado do servidor espera um '\n' pra parar de ler,
é preciso reinserir o delimitador antes de enviar
Possíveis dúvidas: por que o try/catch dentro do loop? porque uma exceção que escapa
da função de uma thread (ex: conexão fechada pelo servidor) chama std::terminate
e derruba o processo inteiro; aqui ela só encerra esta thread specificamente
========================*/
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

/*=======================
O que é: Thread 2 - lê do socket e imprime na tela
O que faz: Em loop infinito, espera dados do servidor e imprime assim que chegam
Como faz: receiveLine() bloqueia até receber uma linha completa (terminada em '\n')
Possíveis dúvidas: mesmo motivo do inputLoop pro try/catch - receiveLine() lança
quando o servidor fecha a conexão, e isso não pode escapar da thread
========================*/
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


// observação se o servidor fechar a conexão, outputLoop percebe na hora, via receiveLine(), e encerra. Porém o inputLoop só vai finalizar na próxima
// vez que o usuário digitar algo e tentar um sendAll. Ver se isso é algo necessário de resolver posteriormente