#include "Lottery.hpp"
#include <algorithm> // shuffle, sort, binary_search, adjacent_find
#include <numeric>   // iota

// Configuração padrão exigida pelo enunciado: "caso a loteria não seja configurada no início
// da execução, ela considerará como de 0 a 100, 5 números sorteados"
static const int sorteioInicioPadrao = 0;
static const int sorteioFimPadrao = 100;
static const int qtdPadrao = 5;

// Teto para o tamanho do intervalo. Existe porque draw() materializa todos os números
// possíveis num vetor antes de embaralhar: sem esse limite, um ":fim 2000000000" faria o
// servidor tentar alocar bilhões de ints e morrer
static const long long intervaloMax = 1000000;

/*=======================
O que é: Construtor
O que faz: Deixa a loteria na configuração padrão do enunciado (0 a 100, 5 números) e cria
 o gerador de números aleatórios
Possíveis dúvidas: por que criar com random_device? Sem semente, o mt19937 parte sempre do
 mesmo estado interno, e todo sorteio de toda execução sairia igual. random_device{}() cria um
 objeto temporário e já chama o operator() dele, devolvendo um número imprevisível
========================*/
Lottery::Lottery()
    : inicio_(sorteioInicioPadrao),
      fim_(sorteioFimPadrao),
      qtd_(qtdPadrao),
      rng_(std::random_device{}())
{
}

/*=======================
O que é: Validador das variáveis de configuração
O que faz: Diz se o trio (inicio, fim, qtd) descreve uma loteria que faz sentido sortear.
 "inicio >= 0" e "qtd >= 1" moram no setInicio/setQtd, por onde esses valores entram
Possíveis dúvidas: por que long long? "fim - inicio + 1" com fim = INT_MAX estoura o int, e
 overflow com sinal é comportamento indefinido. Daria negativo e passaria numa checagem que
 deveria falhar. O cast tem que vir ANTES da subtração. E não vale long: no Windows ele tem os
 mesmos 32 bits do int. Não tranca o mutex: os setters já têm o lock e ele não é reentrante
========================*/
bool Lottery::isValidConfig(int inicio, int fim, int qtd) const
{
    if (inicio >= fim) return false; // intervalo invertido ou de um número só

    const long long intervalo = static_cast<long long>(fim) - inicio + 1; // quantos números existem no intervalo

    if (intervalo > intervaloMax) return false; // intervalo grande demais para materializar em draw()
    if (qtd > intervalo) return false;       // não dá para sortear 10 números distintos de um intervalo de 4

    return true;
}

/*=======================
O que é: Setter do início do intervalo (comando ":inicio N")
O que faz: Muda o menor número que pode ser sorteado, se isso não quebrar a configuração
O que retorna: false (sem alterar nada) se a configuração resultante seria inválida
Como faz: Monta mentalmente o trio que existiria depois da mudança e passa por isValidConfig
Possíveis dúvidas: por que validar o trio inteiro em vez de só o campo que mudou? Porque os três
 campos são interdependentes: com fim = 10 e qtd = 5, um ":inicio 8" deixaria só 3 números no
 intervalo para um sorteio que pede 5. Validar isolado deixaria passar
========================*/
bool Lottery::setInicio(int value)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (value < 0 || !isValidConfig(value, fim_, qtd_)) return false;

    inicio_ = value;
    return true;
}

/*=======================
O que é: Setter do fim do intervalo (comando ":fim N")
O que faz: Muda o maior número que pode ser sorteado, se isso não quebrar a configuração
Como faz: Mesma lógica do setInicio, trocando o campo testado
========================*/
bool Lottery::setFim(int value)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!isValidConfig(inicio_, value, qtd_)) return false;

    fim_ = value;
    return true;
}

/*=======================
O que é: Setter da quantidade de números sorteados (comando ":qtd N")
O que faz: Muda quantos números saem por sorteio, se couberem no intervalo atual
Como faz: Mesma lógica dos outros dois setters
========================*/
bool Lottery::setQtd(int value)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (value < 1 || !isValidConfig(inicio_, fim_, value)) return false;
    // sorteio de zero números não faz sentido

    qtd_ = value;
    return true;
}

/*=======================
O que é: Registra uma aposta do usuário
O que faz: Guarda os números apostados, ou rejeita a aposta INTEIRA se for vazia, tiver mais
 números do que serão sorteados, número fora do intervalo, ou número repetido
Como faz: detecta repetidos ordenando uma cópia e procurando dois iguais lado a lado
Possíveis dúvidas: por que validar aqui se já existe um parser? parseAposta (Protocol.cpp) só
 garante que a linha tem inteiros, aceita "-5" e "999999", pois não conhece a configuração.
 E repetidos dão erro, porque "7 7 7" contaria o mesmo acerto 3 vezes
========================*/
bool Lottery::addAposta(const std::vector<int>& numbers)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // size() é sem sinal e qtd_ é int: sem o cast o compilador avisa (-Wsign-compare).
    // Converter qtd_ é seguro porque isValidConfig garante que ele nunca é negativo.
    if (numbers.empty() || numbers.size() > static_cast<std::size_t>(qtd_)) return false;

    for (int n : numbers)
    {
        if (n < inicio_ || n > fim_) return false; // rejeita a aposta inteira, não só o número
    }

    // adjacent_find só acha repetidos que estão colados, por isso a cópia ordenada primeiro
    std::vector<int> ordenada = numbers;
    std::sort(ordenada.begin(), ordenada.end());
    if (std::adjacent_find(ordenada.begin(), ordenada.end()) != ordenada.end()) return false;
    ///adjacent_find busca por vizinhos repetidos, se não encontrar nenhum, retorna o ordenada.end()

    bets_.push_back(numbers); // guarda na ordem original em que o usuário digitou
    return true;
}

/*=======================
O que é: Um ciclo completo de sorteio
O que faz: Sorteia, confere todas as apostas do ciclo, zera a lista e devolve o resultado
Como faz: iota monta os números do intervalo, shuffle embaralha, pega os qtd_ primeiros
 (distintos de graça) e ordena, o que permite busca binária na conferência
Possíveis dúvidas: por que sortear, conferir e zerar sob o MESMO lock? É o ponto crítico da
 classe: soltar o mutex no meio faria uma aposta que chegasse nesse intervalo ser apagada sem
 nunca ter sido conferida. O lock também protege o rng_, que não é thread-safe. E embaralhar
 evita o sorteio-com-descarte, lento quando qtd_ é perto do tamanho do intervalo
========================*/
DrawResult Lottery::draw()
{
    std::lock_guard<std::mutex> lock(mutex_);

    DrawResult result;

    // conjunto = todos os números que podem sair. O tamanho é seguro por causa do intervaloMax
    std::vector<int> conjunto(static_cast<std::size_t>(fim_ - inicio_ + 1)); //Cria um vetor com o tamanho de todos os valores de inicio até fim
    std::iota(conjunto.begin(), conjunto.end(), inicio_); // preenche o vetor a partir do valor de início e vai completando com +1
    std::shuffle(conjunto.begin(), conjunto.end(), rng_); //embaralha os valores no vetor, 

    result.drawn.assign(conjunto.begin(), conjunto.begin() + qtd_); // os qtd_ primeiros do baralho
    std::sort(result.drawn.begin(), result.drawn.end()); //ordena o vetor dos valores selecionados

    // confere aposta por aposta
    for (const std::vector<int>& aposta : bets_)
    {
        BetResult conferida;
        conferida.numbers = aposta;

        for (int n : aposta)
        {
            // drawn está ordenado, então dá para usar busca binária em vez de varrer tudo
            if (std::binary_search(result.drawn.begin(), result.drawn.end(), n))
            {
                conferida.hits.push_back(n);
            }
        }

        result.bets.push_back(std::move(conferida));
    }

    bets_.clear(); // zera a lista e começa um novo ciclo, como manda o enunciado
    return result;
}

/*=======================
O que é: Leitura da configuração atual
O que faz: Devolve uma cópia dos três parâmetros, para montar mensagens de confirmação
Possíveis dúvidas: por que uma struct em vez de três getters separados? Com getters individuais
 a configuração poderia mudar entre uma chamada e outra, e quem chamou montaria uma mensagem
 com valores que nunca existiram juntos. Copiar os três sob o mesmo lock dá um retrato coerente.
 E por que é const se tranca o mutex? Porque mutex_ é mutable no header, justamente para
 permitir travar dentro de métodos que não alteram o estado lógico do objeto
========================*/
LotteryConfig Lottery::config() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return LotteryConfig{inicio_, fim_, qtd_};
}

/*=======================
O que é: Auxiliar de formatação
O que faz: Junta um vetor de números numa string única separada por espaços ("4 17 23")
Como faz: Percorre por índice, colando um espaço antes de todos menos o primeiro
Possíveis dúvidas: static aqui limita a visibilidade a este .cpp — é detalhe interno do
 formatDrawResult, não precisa aparecer no header
========================*/
static std::string joinNumbers(const std::vector<int>& numbers)
{
    std::string out;
    for (std::size_t i = 0; i < numbers.size(); ++i)
    {
        if (i > 0) out += ' ';
        out += std::to_string(numbers[i]);
    }
    return out;
}

/*=======================
O que é: Tradutor do resultado do sorteio para o texto que o cliente vê
O que faz: Monta as linhas do protocolo: a do sorteio e uma por aposta conferida
Possíveis dúvidas: por que um vetor de linhas em vez de uma string só com '\n' no meio? Para
 deixar explícito que cada elemento é UMA mensagem. Quem envia faz sendAll(linha + "\n") uma
 por uma, e o Socket::receiveLine() do cliente lê até o '\n' e devolve a linha sem ele.
 Esta função NÃO acrescenta o '\n': isso é de quem envia, igual o formatWelcomeMessage()
========================*/
std::vector<std::string> formatDrawResult(const DrawResult& result)
{
    std::vector<std::string> lines;

    lines.push_back("SORTEIO: " + joinNumbers(result.drawn));

    if (result.bets.empty())
    {
        lines.push_back("NENHUMA APOSTA REGISTRADA NESTE CICLO");
        return lines;
    }

    for (std::size_t i = 0; i < result.bets.size(); ++i)
    {
        const BetResult& aposta = result.bets[i];

        // i + 1 porque a numeração mostrada ao usuário começa em 1, não em 0
        std::string line = "APOSTA " + std::to_string(i + 1) + ": " + joinNumbers(aposta.numbers) + " -> ";

        if (aposta.hits.empty())
        {
            line += "NENHUM ACERTO";
        }
        else
        {
            line += "ACERTOU " + std::to_string(aposta.hits.size()) + ": " + joinNumbers(aposta.hits);
        }

        lines.push_back(std::move(line));
    }

    return lines;
}
