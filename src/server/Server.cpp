#include "Server.hpp"
#include "ClientSession.hpp"
#include <iostream>
#include <thread>

/*=======================
O que é: Construtor do servidor
O que faz: Deixa o socket de escuta pronto para receber conexões na porta informada
Como faz: bindTo() reserva a porta na máquina e listenFor() coloca o socket em modo de escuta
Possíveis dúvidas: por que fazer isso aqui e não no run()? Assim o objeto ou nasce pronto ou
 não nasce: se a porta já estiver ocupada, o bindTo lança e o erro aparece lá no main, antes
 de qualquer cliente tentar conectar. O run() fica sendo só o laço.
========================*/
Server::Server(unsigned short port)
    : porta_(port)
{
    listener_.bindTo(port);
    listener_.listenFor();
}

/*=======================
O que é: Laço principal do servidor
O que faz: Aceita conexões para sempre, entregando cada cliente a uma ClientSession própria
Como faz: accept() bloqueia até alguém conectar; o socket devolvido é movido para dentro de uma
 thread nova, que constrói a sessão e chama run() nela
Possíveis dúvidas: por que uma thread por cliente? Porque ClientSession::run() só retorna quando
 aquele cliente desconecta — chamá-lo aqui direto travaria o accept e o servidor atenderia um
 cliente só. E o try/catch DENTRO da lambda é obrigatório: exceção que escapa da função de uma
 thread chama std::terminate e derruba o servidor inteiro, junto com todos os outros clientes.
========================*/
void Server::run()
{
    std::cout << "Servidor escutando na porta " << porta_ << "..." << std::endl;

    while (true)
    {
        try
        {
            Socket cliente = listener_.accept(); // bloqueia até chegar uma conexão
            std::cout << "Novo cliente conectado." << std::endl;

            // [s = std::move(cliente)] é captura por movimento: Socket não é copiável, então
            // não dá para capturar por valor do jeito comum. O "mutable" é obrigatório porque
            // sem ele "s" seria const dentro da lambda, e não poderia ser movido de novo.
            std::thread([s = std::move(cliente)]() mutable {
                try
                {
                    ClientSession sessao(std::move(s));
                    sessao.run(); // só retorna quando este cliente cair
                }
                catch (const std::exception& e)
                {
                    std::cerr << "Sessao encerrada: " << e.what() << std::endl;
                }
            }).detach(); // solta a thread: ela se encerra sozinha quando o cliente sai
        }
        catch (const std::exception& e)
        {
            // accept() lança quando o listener é fechado — é por aqui que o servidor termina
            std::cerr << "Servidor encerrando: " << e.what() << std::endl;
            break;
        }
    }
}
