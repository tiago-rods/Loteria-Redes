#include "Server.hpp"
#include "ClientSession.hpp"
#include <iostream>
#include <string>
#include <thread>

// Mensagem enviada NO LUGAR da MSG1 quando o limite já foi atingido. É const char* e não
// std::string de propósito: um std::string global teria destrutor rodando na saída do
// processo, com threads detached possivelmente ainda lendo a string nesse instante.
static const char* const MSG_LIMITE = "ERRO: LIMITE DE CLIENTES ATINGIDO";

/*=======================
O que é: Guard RAII da vaga de cliente
O que faz: Devolve a vaga ao servidor quando sai de escopo, aconteça o que acontecer
Como faz: guarda uma referência ao Server e chama releaseSlot() no destrutor
Possíveis dúvidas: por que não chamar releaseSlot() na mão no fim da thread? Porque se
 ClientSession::run() lançar, a linha seria pulada e a vaga ficaria ocupada até o servidor
 reiniciar. O destrutor roda mesmo durante a propagação da exceção. Fica em namespace anônimo
 porque "static" não se aplica a definição de classe: o anônimo é o equivalente para tipos
========================*/
namespace
{
    struct SlotGuard
    {
        Server& servidor_;

        explicit SlotGuard(Server& servidor) : servidor_(servidor) {}
        ~SlotGuard() { servidor_.releaseSlot(); }

        SlotGuard(const SlotGuard&) = delete;            // uma vaga, um guard: copiar levaria
        SlotGuard& operator=(const SlotGuard&) = delete; // a liberar a mesma vaga duas vezes
    };
}

/*=======================
O que é: Construtor do servidor
O que faz: Deixa o socket de escuta pronto e guarda o teto de clientes simultâneos
Como faz: bindTo() reserva a porta na máquina e listenFor() coloca o socket em modo de escuta;
 o maxClientes já vem validado do main e o contador de ativos nasce zerado
Possíveis dúvidas: por que fazer isso aqui e não no run()? Assim o objeto ou nasce pronto ou
 não nasce: se a porta já estiver ocupada, o bindTo lança e o erro aparece lá no main, antes
 de qualquer cliente tentar conectar. O run() fica sendo só o laço.
========================*/
Server::Server(unsigned short port, int maxClientes)
    : porta_(port), maxClientes_(maxClientes)
{
    listener_.bindTo(port);
    listener_.listenFor();
}

/*=======================
O que é: Tentativa de ocupar uma das vagas de cliente
O que faz: Soma 1 ao contador de ativos só se ainda houver vaga, e diz se conseguiu
Como faz: laço de compare_exchange_weak, que só grava atual+1 se o contador ainda valer
 exatamente "atual"; quando falha, ele mesmo recarrega "atual" e o laço reavalia a condição
Possíveis dúvidas: por que não "if (ativos < max) ativos++"? Entre o teste e a soma outra
 thread caberia no meio e as duas passariam do limite. O CAS funde testar-e-somar numa
 operação só, então o contador nunca ultrapassa maxClientes_, nem por um instante
========================*/
bool Server::tryAcquireSlot() noexcept
{
    int atual = clientesAtivos_.load();

    while (atual < maxClientes_)
    {
        if (clientesAtivos_.compare_exchange_weak(atual, atual + 1))
        {
            return true;
        }
    }

    return false;
}

/*=======================
O que é: Devolução da vaga quando o atendimento termina
O que faz: Subtrai 1 do contador de ativos, liberando a posição para o próximo cliente
Como faz: fetch_sub(1) no atômico, que é indivisível e dispensa mutex
Possíveis dúvidas: por que não tem laço como o tryAcquireSlot? Porque não há condição a
 verificar na devolução — a vaga já é nossa. E é noexcept porque roda dentro de um destrutor:
 exceção escapando de destrutor durante outra exceção chama std::terminate
========================*/
void Server::releaseSlot() noexcept
{
    clientesAtivos_.fetch_sub(1);
}

/*=======================
O que é: Laço principal do servidor
O que faz: Aceita conexões para sempre e entrega cada uma a uma working thread própria
Como faz: accept() bloqueia até alguém conectar; o socket é movido para dentro da thread, que
 decide se há vaga (tryAcquireSlot) antes de abrir a sessão
Possíveis dúvidas: por que a decisão de aceitar/recusar fica na thread e não aqui? Porque o
 enunciado manda a linha principal voltar imediatamente ao accept(), e a recusa faz E/S de
 rede. Capturar "this" é seguro: o Server vive no main durante todo o run(), e nenhuma thread
 nasce depois que ele retorna
========================*/
void Server::run()
{
    std::cout << "Servidor escutando na porta " << porta_
              << " (limite de " << maxClientes_ << " clientes)..." << std::endl;

    while (true)
    {
        try
        {
            Socket cliente = listener_.accept(); // bloqueia até chegar uma conexão

            // [s = std::move(cliente)] é captura por movimento: Socket não é copiável, então
            // não dá para capturar por valor do jeito comum. O "mutable" é obrigatório porque
            // sem ele "s" seria const dentro da lambda, e não poderia ser movido de novo.
            // O "this" entra só para dar acesso ao contador de vagas.
            std::thread([this, s = std::move(cliente)]() mutable {
                if (!tryAcquireSlot())
                {
                    // limite atingido: a recusa vai NO LUGAR da MSG1. O '\n' entra na mão
                    // porque este caminho não passa pelo ClientSession::sendLine(), que é
                    // quem normalmente põe o delimitador do protocolo
                    try
                    {
                        s.sendAll(std::string(MSG_LIMITE) + "\n");
                    }
                    catch (const std::exception&)
                    {
                        // o cliente pode ter sumido antes de ler; não há mais nada a fazer
                    }

                    // closesocket() com o SO_LINGER padrão é fechamento gracioso: manda o que
                    // está na fila e só então o FIN, então a mensagem acima chega ao cliente
                    s.close();

                    std::cout << "Conexao recusada: limite de clientes atingido." << std::endl;
                    return; // a thread morre aqui, sem nunca ter ocupado vaga
                }

                // a vaga é nossa. O guard a devolve em qualquer saída deste escopo, e como
                // ele é declarado ANTES do try, a sessão (e o socket dela) morre primeiro:
                // a vaga só volta depois que a conexão fechou de verdade
                SlotGuard vaga(*this);
                std::cout << "Novo cliente conectado." << std::endl;

                try
                {
                    ClientSession sessao(std::move(s));
                    sessao.run(); // só retorna quando este cliente cair
                }
                catch (const std::exception& e)
                {
                    // exceção que escapa da função de uma thread chama std::terminate e
                    // derrubaria o servidor inteiro, junto com todos os outros clientes
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
