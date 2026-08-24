#pragma once
#include "../common/Socket.hpp"

// Servidor TCP do jogo. A única função dele é aceitar conexões e entregar cada uma a uma
// ClientSession, que roda numa thread própria.
//
// Como a decisão do projeto é UMA LOTERIA POR CLIENTE, cada sessão é independente: tem a
// própria Lottery e o próprio sorteio de 1 em 1 minuto. Por isso o Server não guarda lista de
// sessões, não precisa de mutex e não conhece loteria nenhuma — num modelo de sorteio global
// todas essas coisas teriam que existir aqui, para o broadcast do resultado.

class Server
{
public:
    // Já deixa o socket escutando na porta; lança se ela estiver ocupada
    explicit Server(unsigned short port);

    // Laço de accept. Bloqueia até o listener falhar ou ser fechado.
    void run();

private:
    Socket listener_;      // socket que só escuta; nunca troca dados com ninguém
    unsigned short porta_; // guardado só para a mensagem de log
};
