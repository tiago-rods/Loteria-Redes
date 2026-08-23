#pragma once
#include <mutex>
#include <random>
#include <string>
#include <vector>

// Estado e regras do sorteio.
//
// Esta classe NÃO conhece sockets nem threads: ela só guarda a configuração e as apostas,
// e sabe sortear e conferir. Quem cuida da rede (ClientSession) e quem cuida do relógio de
// 1 minuto (a thread 2 do servidor) ficam de fora daqui.
//
// Toda a sincronização é INTERNA: todo método público tranca o mutex sozinho, então quem usa
// a classe não precisa travar nada por fora. É isso que permite que a thread que recebe as
// apostas e a thread que faz o sorteio mexam no mesmo objeto sem corromper o estado.
//
// Como a classe não decide QUANDO o sorteio acontece, ela funciona igual sendo uma instância
// por cliente ou uma instância global compartilhada por todos — essa escolha fica com quem
// cria o objeto, e não precisa ser tomada agora.

// resultado da conferência de UMA aposta
struct BetResult
{
    std::vector<int> numbers; // a aposta exatamente como foi feita
    std::vector<int> hits;    // quais números dela saíram no sorteio
};

// resultado completo de um ciclo de sorteio
struct DrawResult
{
    std::vector<int> drawn;      // os números sorteados, em ordem crescente
    std::vector<BetResult> bets; // vazio se ninguém apostou neste ciclo
};

// cópia da configuração, para quem quiser consultar sem segurar o mutex
struct LotteryConfig
{
    int inicio;
    int fim;
    int qtd;
};

class Lottery
{
public:
    Lottery(); // começa na configuração padrão do enunciado: 0 a 100, 5 números

    // Cada setter valida a configuração INTEIRA que resultaria da mudança, não só o campo
    // alterado. Retornam false e não mexem em nada se o resultado seria inválido.
    bool setInicio(int value);
    bool setFim(int value);
    bool setQtd(int value);

    // Registra uma aposta. Retorna false se a aposta for vazia, tiver números repetidos, ou
    // tiver algum número fora do intervalo configurado no momento.
    bool addAposta(const std::vector<int>& numbers);

    // Sorteia, confere todas as apostas e zera a lista — tudo numa operação só.
    DrawResult draw();

    LotteryConfig config() const; // snapshot da configuração atual

private:
    // Checa os invariantes de uma configuração candidata. Não tranca o mutex: é chamada
    // pelos setters, que já estão segurando o lock.
    bool isValidConfig(int inicio, int fim, int qtd) const;

    // mutable permite trancar o mutex dentro de config(), que é const
    mutable std::mutex mutex_;

    int inicio_;
    int fim_;
    int qtd_;

    std::vector<std::vector<int>> bets_; // cada elemento é uma aposta completa
    std::mt19937 rng_;                   // gerador de números pseudoaleatórios
};

// Transforma o resultado do sorteio nas linhas de texto que vão para o cliente.
// Devolve um vetor onde CADA elemento é uma linha do protocolo: quem envia deve mandar uma
// por uma com sendAll(linha + "\n"), já que Socket::receiveLine() do outro lado usa o '\n'
// como delimitador de mensagem e essa função não acrescenta o '\n' sozinha.
std::vector<std::string> formatDrawResult(const DrawResult& result);
