// Teste da classe Lottery.
//
// Não faz parte do build do projeto: é um executável separado, com main() próprio, que exercita
// a Lottery sem subir servidor nem abrir socket nenhum. Isso só é possível porque a Lottery não
// inclui winsock2.h — ela é pura regra de jogo — então compila sozinha, sem -lws2_32.
//
// Como rodar (a partir da raiz do repositório):
//   g++ -std=c++20 -Wall -Wextra tests/test_lottery.cpp src/server/Lottery.cpp -o tests/test_lottery.exe
//   ./tests/test_lottery.exe
//
// Sai com código 0 se tudo passou, 1 se algo falhou.

#include "../src/server/Lottery.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <vector>

static int falhas = 0;

static void check(bool ok, const std::string& nome)
{
    std::cout << (ok ? "  ok   " : "  FALHOU ") << nome << "\n";
    if (!ok) ++falhas;
}

int main()
{
    // ---- 1. configuração padrão ----
    std::cout << "[1] configuracao padrao\n";
    {
        Lottery l;
        LotteryConfig c = l.config();
        check(c.inicio == 0 && c.fim == 100 && c.qtd == 5, "padrao e 0..100, 5 numeros");

        DrawResult r = l.draw();
        check(r.drawn.size() == 5, "sorteia 5 numeros");

        std::set<int> distintos(r.drawn.begin(), r.drawn.end());
        check(distintos.size() == 5, "os 5 sao distintos");

        bool naFaixa = std::all_of(r.drawn.begin(), r.drawn.end(),
                                   [](int n) { return n >= 0 && n <= 100; });
        check(naFaixa, "todos dentro de 0..100");
        check(std::is_sorted(r.drawn.begin(), r.drawn.end()), "vem ordenado");
        check(r.bets.empty(), "sem apostas -> bets vazio");
    }

    // ---- 2. validação de configuração ----
    std::cout << "[2] validacao de config\n";
    {
        Lottery l;
        check(!l.setQtd(200), "qtd 200 num intervalo de 101 falha");
        check(!l.setQtd(0), "qtd 0 falha");
        check(!l.setFim(3), "fim 3 com qtd 5 falha (so 4 numeros)");
        check(!l.setInicio(100), "inicio == fim falha");
        check(!l.setInicio(150), "inicio > fim falha");
        check(!l.setInicio(-1), "inicio negativo rejeitado");

        LotteryConfig c = l.config();
        check(c.inicio == 0 && c.fim == 100 && c.qtd == 5, "nada mudou apos as falhas");

        check(l.setInicio(50), "inicio 50 passa");
        check(l.setFim(60), "fim 60 passa");
        c = l.config();
        check(c.inicio == 50 && c.fim == 60 && c.qtd == 5, "config aplicada");

        check(l.setQtd(11), "qtd 11 num intervalo de 11 passa (limite exato)");
        check(!l.setQtd(12), "qtd 12 num intervalo de 11 falha");

        // a config nova tem que valer para o proximo sorteio
        l.setQtd(11);
        DrawResult r = l.draw();
        check(r.drawn.size() == 11, "sorteio usa a config nova");
        bool naFaixa = std::all_of(r.drawn.begin(), r.drawn.end(),
                                   [](int n) { return n >= 50 && n <= 60; });
        check(naFaixa, "sorteio respeita o intervalo novo");
    }

    // ---- 3. validação de apostas ----
    std::cout << "[3] validacao de apostas\n";
    {
        Lottery l; // 0..100
        check(!l.addAposta({}), "aposta vazia rejeitada");
        check(!l.addAposta({-5}), "numero negativo fora da faixa rejeitado");
        check(!l.addAposta({999999}), "numero acima do fim rejeitado");
        check(!l.addAposta({1, 2, 200}), "aposta inteira cai se um numero e invalido");
        check(!l.addAposta({7, 7, 7}), "numeros repetidos rejeitados");
        check(l.addAposta({0}), "limite inferior aceito");
        check(l.addAposta({100}), "limite superior aceito");
        check(l.addAposta({1, 2, 3}), "aposta normal aceita");
    }

    // ---- 4. conferência e limpeza do ciclo ----
    std::cout << "[4] conferencia e ciclo\n";
    {
        Lottery l;
        // intervalo 1..5 sorteando 5 => TODOS os numeros saem, entao os acertos sao deterministicos
        l.setInicio(1);
        l.setFim(5);
        l.setQtd(5);

        check(l.addAposta({1, 3}), "aposta 1 registrada");
        check(l.addAposta({2, 4, 5}), "aposta 2 registrada");

        DrawResult r = l.draw();
        check(r.bets.size() == 2, "duas apostas conferidas");
        check(r.bets[0].numbers == std::vector<int>({1, 3}), "aposta 1 preservada como digitada");
        check(r.bets[0].hits.size() == 2, "aposta 1 acertou os 2 (todo numero sai)");
        check(r.bets[1].hits.size() == 3, "aposta 2 acertou os 3");

        DrawResult r2 = l.draw();
        check(r2.bets.empty(), "lista zerada apos o sorteio");
    }

    // ---- 5. tamanho da aposta limitado por qtd ----
    std::cout << "[5] tamanho da aposta vs qtd\n";
    {
        Lottery l;
        l.setInicio(1);
        l.setFim(10);
        l.setQtd(1); // so 1 numero sai de 10, entao a aposta so pode ter 1 numero

        check(!l.addAposta({1, 2, 3}), "aposta maior que qtd rejeitada");
        check(l.addAposta({1}), "aposta do tamanho de qtd aceita");

        DrawResult r = l.draw();
        check(r.drawn.size() == 1, "sorteou 1 numero");
        check(r.bets.size() == 1, "so a aposta valida foi registrada");
        check(r.bets[0].hits.size() <= 1, "no maximo 1 acerto possivel");
    }

    // ---- 6. formatação das linhas ----
    std::cout << "[6] formatacao\n";
    {
        DrawResult r;
        r.drawn = {4, 17, 23, 88, 91};

        std::vector<std::string> semApostas = formatDrawResult(r);
        check(semApostas.size() == 2, "sem apostas -> 2 linhas");
        check(semApostas[0] == "SORTEIO: 4 17 23 88 91", "linha do sorteio");
        check(semApostas[1] == "NENHUMA APOSTA REGISTRADA NESTE CICLO", "aviso de ciclo vazio");

        r.bets.push_back(BetResult{{3, 17, 23}, {17, 23}});
        r.bets.push_back(BetResult{{1, 2}, {}});
        std::vector<std::string> comApostas = formatDrawResult(r);
        check(comApostas.size() == 3, "1 linha de sorteio + 2 de aposta");
        check(comApostas[1] == "APOSTA 1: 3 17 23 -> ACERTOU 2: 17 23", "linha com acertos");
        check(comApostas[2] == "APOSTA 2: 1 2 -> NENHUM ACERTO", "linha sem acertos");

        // cada elemento tem que ser UMA linha: quem envia e que poe o '\n'
        bool semQuebra = true;
        for (const std::string& s : comApostas)
            if (s.find('\n') != std::string::npos) semQuebra = false;
        check(semQuebra, "nenhuma linha contem '\\n' (quem envia que poe)");

        std::cout << "    --- amostra do que o cliente veria ---\n";
        for (const std::string& s : comApostas) std::cout << "    " << s << "\n";
    }

    // ---- 7. duas threads mexendo no mesmo objeto ----
    // Teste leve: garante que nao trava e que nenhuma aposta some entre o sorteio e o clear.
    // Nao e teste de carga - a thread do sorteio termina antes da de aposta ir longe.
    std::cout << "[7] concorrencia (addAposta + draw em paralelo)\n";
    {
        Lottery l;
        std::atomic<bool> parar{false};
        std::atomic<int> aceitas{0};
        std::atomic<int> conferidas{0};

        std::thread apostador([&] {
            while (!parar)
                for (int n = 0; n <= 100; ++n)
                    if (l.addAposta({n})) ++aceitas;
        });

        std::thread sorteador([&] {
            for (int i = 0; i < 200; ++i)
            {
                DrawResult r = l.draw();
                conferidas += static_cast<int>(r.bets.size());
            }
            parar = true;
        });

        apostador.join();
        sorteador.join();

        // sorteio final para conferir o que sobrou na lista
        DrawResult sobra = l.draw();
        conferidas += static_cast<int>(sobra.bets.size());

        check(true, "rodou sem crash nem deadlock");
        check(aceitas == conferidas,
              "nenhuma aposta perdida (aceitas=" + std::to_string(aceitas) +
                  " conferidas=" + std::to_string(conferidas) + ")");
    }

    std::cout << "\n"
              << (falhas == 0 ? "TODOS OS TESTES PASSARAM"
                              : std::to_string(falhas) + " TESTE(S) FALHARAM")
              << "\n";
    return falhas == 0 ? 0 : 1;
}
