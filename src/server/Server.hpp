#pragma once
#include "../common/Socket.hpp"
#include <atomic>

// Servidor TCP do jogo. A única função dele é aceitar conexões e entregar cada uma a uma
// ClientSession, que roda numa thread própria.
//
// Como a decisão do projeto é UMA LOTERIA POR CLIENTE, cada sessão é independente: tem a
// própria Lottery e o próprio sorteio de 1 em 1 minuto. Por isso o Server não guarda lista de
// sessões e não conhece loteria nenhuma, num modelo de sorteio global todas essas coisas
// teriam que existir aqui, para o broadcast do resultado.
//
// O ÚNICO estado que todas as working threads enxergam é o contador de clientes ativos: é a
// "memória compartilhada válida para todas as threads" que a Fase 2 do enunciado cita, em
// oposição aos dados de cada cliente, que ficam dentro da ClientSession dele. Como é um
// contador só, um std::atomic dá conta e continua não sendo preciso nenhum mutex aqui.

class Server
{
public:
    // Já deixa o socket escutando na porta; lança se ela estiver ocupada.
    // maxClientes é o teto de sessões simultâneas, já validado no main.
    Server(unsigned short port, int maxClientes);

    // Laço de accept. Bloqueia até o listener falhar ou ser fechado.
    void run();

    // Ocupa uma vaga se ainda houver; devolve false se o servidor já está cheio.
    // Este par é público porque quem chama é o SlotGuard do Server.cpp, que não é membro
    // da classe: ele vive num namespace anônimo, então nem "friend" o alcançaria.
    bool tryAcquireSlot() noexcept;

    // Devolve a vaga ocupada por um tryAcquireSlot() que deu certo.
    void releaseSlot() noexcept;

private:
    Socket listener_;                    // socket que só escuta; nunca troca dados com ninguém
    unsigned short porta_;               // guardado só para a mensagem de log
    int maxClientes_;                    // teto vindo do argv; não muda depois do construtor
    std::atomic<int> clientesAtivos_{0}; // lido e escrito por TODAS as working threads
};
