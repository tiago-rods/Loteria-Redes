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
O que faz: Deixa a loteria pronta para uso já na configuração padrão do enunciado (0 a 100,
 5 números), e semeia o gerador de números aleatórios
Como faz: Lista de inicialização preenche os três campos de configuração; rng_ recebe uma
 semente de std::random_device, que é uma fonte de entropia do sistema operacional
Possíveis dúvidas: por que semear com random_device em vez de usar o mt19937 direto? Sem
 semente, o mt19937 começa sempre do mesmo estado interno, e todo sorteio de toda execução
 sairia exatamente igual. random_device{}() cria um objeto temporário e já chama o operator()
 dele, devolvendo um número imprevisível para servir de ponto de partida
========================*/
Lottery::Lottery()
    : inicio_(sorteioInicioPadrao),
      fim_(sorteioFimPadrao),
      qtd_(qtdPadrao),
      rng_(std::random_device{}())
{
}

/*=======================
O que é: Validador dos invariantes da configuração
O que faz: Diz se um trio (inicio, fim, qtd) descreve uma loteria que faz sentido sortear
Como faz: Checa que o intervalo não está invertido, que se pede pelo menos 1 número, que o
 intervalo cabe no teto de memória, e que não se pede mais números distintos do que existem
 no intervalo
Possíveis dúvidas: por que calcular range em long? Porque "fim - inicio + 1" com ints
 extremos (ex: se fim = INT_MAX, intervalo = INT_MAX + 0 + 1) estoura a faixa do int causando
 overflow. Promover para long antes da subtração evita isso.
 Esta função NÃO tranca o mutex de propósito: quem chama (os setters) já está segurando o lock,
 e std::mutex não é reentrante — trancar de novo aqui travaria a thread para sempre
========================*/
bool Lottery::isValidConfig(int inicio, int fim, int qtd) const
{
    if (inicio >= fim) return false; // intervalo invertido ou de um número só

    const long long intervalo = static_cast<long long>(fim) - inicio + 1; // quantos números existem no intervalo

    if (intervalo > intervaloMax) return false; // intervalo grande demais para materializar em draw()
    if (qtd > intervalo) return false;       // não dá para sortear 10 números distintos de um intervalo de 4
    //Se qtd == intervalo, vai simplesmente sortear todos os possíveis valores

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
O que faz: Guarda a lista de números apostados, ou rejeita a aposta inteira se ela não fizer
 sentido para a configuração atual
O que retorna: false se a aposta é vazia, tem número fora do intervalo, ou tem número repetido
Como faz: Checa vazio, percorre validando a faixa de cada número, e detecta repetidos ordenando
 uma cópia e procurando dois elementos iguais lado a lado com adjacent_find
Possíveis dúvidas: por que validar aqui, se já existe um parser? Porque parseAposta (Protocol.cpp)
 só garante que a linha é composta de inteiros — ela aceita "-5" e "999999", porque não
 conhece a configuração da loteria. Este é o único ponto do código que sabe qual é o intervalo
 válido, então a checagem de faixa tem que morar aqui.
 E por que rejeitar repetidos? Porque o sorteio devolve números distintos, então uma aposta
 "7 7 7" contaria o mesmo acerto três vezes e reportaria "ACERTOU 3" com um número só
========================*/
bool Lottery::addAposta(const std::vector<int>& numbers)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (numbers.empty() || numbers.size() > qtd_) return false;

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
O que faz: Sorteia os números, confere todas as apostas do ciclo, zera a lista e devolve o
 resultado para quem for enviar ao cliente
Como faz: Monta um vetor com todos os números possíveis do intervalo (iota), embaralha
 (shuffle) e pega os qtd_ primeiros. Ordena o resultado para ficar legível e para permitir
 busca binária. Depois percorre cada aposta marcando quais números dela saíram
Possíveis dúvidas: por que sortear, conferir e zerar tudo sob o MESMO lock? Esse é o ponto
 crítico da classe. Se o mutex fosse solto entre o sorteio e o bets_.clear(), uma aposta que
 chegasse nesse intervalo de tempo seria apagada sem nunca ter sido conferida — o usuário
 apostaria e o número simplesmente sumiria. Manter tudo numa seção crítica só torna o ciclo
 atômico: de fora, ou o sorteio ainda não aconteceu, ou já aconteceu por inteiro.
 Outro motivo para o lock: std::mt19937 guarda estado interno que muda a cada número gerado,
 então não é thread-safe — duas threads sorteando ao mesmo tempo corromperiam esse estado.
 E por que embaralhar tudo em vez de gerar números aleatórios até juntar qtd_ distintos? Porque
 sortear com repetição e descartar fica lento quando qtd_ é perto do tamanho do intervalo (os
 últimos números demoram muito a "cair"); o embaralhamento é sempre linear e nunca repete
========================*/
DrawResult Lottery::draw()
{
    std::lock_guard<std::mutex> lock(mutex_);

    DrawResult result;

    // conjunto = todos os números que podem sair. O tamanho é seguro por causa do intervaloMax
    std::vector<int> conjunto(static_cast<std::size_t>(fim_ - inicio_ + 1));
    std::iota(conjunto.begin(), conjunto.end(), inicio_); // preenche com inicio_, inicio_+1, ...
    std::shuffle(conjunto.begin(), conjunto.end(), rng_);

    result.drawn.assign(conjunto.begin(), conjunto.begin() + qtd_); // os qtd_ primeiros do baralho
    std::sort(result.drawn.begin(), result.drawn.end());

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
O que faz: Devolve uma cópia dos três parâmetros, para quem quiser montar mensagens de
 confirmação ("intervalo agora é 0 a 50")
Como faz: Tranca o mutex, copia os campos para uma struct e devolve
Possíveis dúvidas: por que devolver uma struct em vez de três getters separados? Porque com
 getters individuais a configuração poderia mudar entre uma chamada e outra, e quem chamou
 montaria uma mensagem com valores que nunca existiram juntos. Copiar os três de uma vez sob
 o mesmo lock garante um retrato coerente.
 E por que o método é const se ele tranca o mutex? Porque mutex_ é declarado mutable no header,
 justamente para permitir travar dentro de métodos que não alteram o estado lógico do objeto
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
Como faz: Primeira linha sempre com os números sorteados; se não houve apostas no ciclo, avisa
 isso e para por aí; senão, uma linha por aposta dizendo quantos e quais números acertou
Possíveis dúvidas: por que devolver um vetor de linhas em vez de uma string só com '\n' no
 meio? Para deixar explícito que cada elemento é UMA mensagem do protocolo. Quem envia faz
 sendAll(linha + "\n") uma por uma, e o Socket::receiveLine() do cliente — que lê até o '\n'
 e devolve a linha sem ele — recebe cada uma separadamente e imprime na hora.
 Repare que esta função não acrescenta o '\n': isso é responsabilidade de quem envia, igual
 acontece com o formatWelcomeMessage() lá no Protocol.cpp
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
