// Teste de integração ponta a ponta.
//
// Sobe o bin/server.exe de verdade num processo separado, conecta como cliente real por TCP e
// valida o protocolo inteiro: MSG1, comandos, apostas, isolamento entre clientes e um sorteio
// completo. Não usa mock nenhum — é o servidor real respondendo.
//
// Como rodar (a partir da raiz do repositório, com bin/server.exe já compilado):
//   make test-e2e
//
// Ou na mão (a barra invertida no fim da linha nao pode aparecer em comentario //, entao
// esta em uma linha so):
//   g++ -std=c++20 -Wall -Wextra tests/test_integration.cpp src/common/Socket.cpp src/common/WinsockGuard.cpp -o bin/test_integration.exe -lws2_32
//
// Demora ~65s por causa do teste de sorteio, que espera o ciclo real de 1 minuto.

#include "../src/common/Socket.hpp"
#include "../src/common/WinsockGuard.hpp"

#include <chrono>
#include <future>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>

static int falhas = 0;

static void check(bool ok, const std::string& nome)
{
    std::cout << (ok ? "  ok   " : "  FALHOU ") << nome << "\n";
    if (!ok) ++falhas;
}

static void checkIgual(const std::string& obtido, const std::string& esperado, const std::string& nome)
{
    bool ok = (obtido == esperado);
    std::cout << (ok ? "  ok   " : "  FALHOU ") << nome << "\n";
    if (!ok)
    {
        std::cout << "         esperado: '" << esperado << "'\n";
        std::cout << "         obtido:   '" << obtido << "'\n";
        ++falhas;
    }
}

// manda uma linha e devolve a resposta do servidor
static std::string pedir(Socket& s, const std::string& comando)
{
    s.sendAll(comando + "\n");
    return s.receiveLine();
}

/*=======================
O que é: Auxiliar que sobe o servidor
O que faz: Executa bin/server.exe na porta indicada, num processo separado
Como faz: CreateProcessA com a saída redirecionada para NUL, para os logs do servidor não
 se misturarem com a saída do teste. maxClientes só entra na linha de comando se for > 0,
 assim as chamadas que não se importam com o limite continuam usando o padrão do servidor
Possíveis dúvidas: por que processo separado em vez de instanciar Server aqui? Porque assim o
 teste exercita o binário real, incluindo o main() e o WinsockGuard dele — é o mesmo executável
 que vai ser entregue, não uma montagem diferente feita só para testar
========================*/
static PROCESS_INFORMATION subirServidor(unsigned short porta, int maxClientes = 0)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE nul = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ,
                             &sa, OPEN_EXISTING, 0, nullptr);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = nul;
    si.hStdError = nul;

    PROCESS_INFORMATION pi{};

    std::string cmd = "bin\\server.exe " + std::to_string(porta);
    if (maxClientes > 0) cmd += " " + std::to_string(maxClientes);

    std::vector<char> linha(cmd.begin(), cmd.end());
    linha.push_back('\0'); // CreateProcessA exige buffer modificável e terminado em nulo

    if (!CreateProcessA(nullptr, linha.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        throw std::runtime_error("nao consegui executar bin\\server.exe - compile com 'make server' antes");
    }

    return pi;
}

int main()
{
    const unsigned short PORTA = 54321;
    const unsigned short PORTA_LIMITE = 54322; // servidor separado, só para o teste do limite

    WinsockGuard guard;

    std::cout << "subindo bin\\server.exe na porta " << PORTA << "...\n";
    PROCESS_INFORMATION servidor = subirServidor(PORTA);
    std::this_thread::sleep_for(std::chrono::milliseconds(700)); // tempo para o bind/listen

    // preenchido na secao [5]; fica aqui fora para ser encerrado mesmo se o teste lancar
    PROCESS_INFORMATION servidorLimite{};

    try
    {
        // ---- 1. conexão e mensagem de boas-vindas ----
        std::cout << "[1] conexao e MSG1\n";
        Socket a;
        a.connectTo("127.0.0.1", PORTA);
        std::string welcome = a.receiveLine();

        // "HH:MM:SS" = 8 caracteres, ": CONECTADO!!" = 13, total 21
        check(welcome.size() == 21, "MSG1 tem o tamanho de 'HH:MM:SS: CONECTADO!!'");
        check(welcome.substr(8) == ": CONECTADO!!", "MSG1 termina com ': CONECTADO!!'");
        check(welcome[2] == ':' && welcome[5] == ':', "MSG1 comeca com HH:MM:SS");
        std::cout << "       recebido: '" << welcome << "'\n";

        // ---- 2. comandos de configuracao ----
        std::cout << "[2] comandos de configuracao\n";
        checkIgual(pedir(a, ":inicio 1"), "INICIO CONFIGURADO: 1", "':inicio 1' aceito");
        checkIgual(pedir(a, ":fim 10"), "FIM CONFIGURADO: 10", "':fim 10' aceito");
        checkIgual(pedir(a, ":qtd 3"), "QUANTIDADE CONFIGURADA: 3", "':qtd 3' aceito");
        checkIgual(pedir(a, ":inicio -5"), "ERRO: INICIO INVALIDO", "inicio negativo recusado");
        checkIgual(pedir(a, ":qtd 999"), "ERRO: QUANTIDADE INVALIDA", "qtd maior que o intervalo recusada");
        checkIgual(pedir(a, ":fim 0"), "ERRO: FIM INVALIDO", "fim menor que inicio recusado");

        // ---- 3. apostas ----
        std::cout << "[3] apostas\n";
        checkIgual(pedir(a, "1 2 3"), "APOSTA REGISTRADA", "aposta valida aceita");
        checkIgual(pedir(a, "1 2 3 4"), "ERRO: APOSTA INVALIDA", "aposta maior que qtd recusada");
        checkIgual(pedir(a, "5 5"), "ERRO: APOSTA INVALIDA", "numeros repetidos recusados");
        checkIgual(pedir(a, "99"), "ERRO: APOSTA INVALIDA", "numero fora do intervalo recusado");
        checkIgual(pedir(a, "abc"), "ERRO: COMANDO OU APOSTA INVALIDA", "lixo recusado");
        checkIgual(pedir(a, ":naoexiste 5"), "ERRO: COMANDO OU APOSTA INVALIDA", "comando inexistente recusado");

        // ---- 4. cada cliente tem a sua propria loteria ----
        std::cout << "[4] loterias independentes\n";
        Socket b;
        b.connectTo("127.0.0.1", PORTA);
        b.receiveLine(); // MSG1 do segundo cliente

        // "50" e invalido no cliente A (1..10) mas valido no B, que esta no padrao 0..100
        checkIgual(pedir(b, "50"), "APOSTA REGISTRADA", "cliente B usa a config padrao 0..100");
        checkIgual(pedir(a, "50"), "ERRO: APOSTA INVALIDA", "cliente A continua limitado a 1..10");
        checkIgual(pedir(b, ":qtd 2"), "QUANTIDADE CONFIGURADA: 2", "config do B e aceita");
        checkIgual(pedir(a, "1 2 3"), "APOSTA REGISTRADA", "qtd 3 do A nao foi afetada pelo B");

        // ---- 5. limite de clientes simultaneos ----
        std::cout << "[5] limite de clientes\n";

        // servidor proprio, em porta separada e com limite 1, para nao interferir no principal
        servidorLimite = subirServidor(PORTA_LIMITE, 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(700));

        {
            Socket x;
            x.connectTo("127.0.0.1", PORTA_LIMITE);
            check(x.receiveLine().substr(8) == ": CONECTADO!!",
                  "cliente dentro do limite recebe a MSG1");

            Socket y;
            y.connectTo("127.0.0.1", PORTA_LIMITE);
            checkIgual(y.receiveLine(), "ERRO: LIMITE DE CLIENTES ATINGIDO",
                       "cliente acima do limite e recusado no lugar da MSG1");

            bool caiu = false;
            try { y.receiveLine(); } catch (const std::exception&) { caiu = true; }
            check(caiu, "servidor encerra a conexao do cliente recusado");

            // a vaga so volta quando a sessao inteira termina: o drawLoop do servidor so olha a
            // flag de 1 em 1 segundo, e run() faz join das duas threads antes de retornar
            x.close();
            std::this_thread::sleep_for(std::chrono::seconds(2));

            Socket z;
            z.connectTo("127.0.0.1", PORTA_LIMITE);
            check(z.receiveLine().substr(8) == ": CONECTADO!!",
                  "vaga e liberada quando um cliente desconecta");
        }

        // ---- 6. sorteio de verdade (espera o ciclo de 1 minuto) ----
        std::cout << "[6] sorteio (aguardando o ciclo real de 60s...)\n";

        // le em outra thread para o teste nao ficar preso para sempre se o sorteio nao vier
        auto leitura = std::async(std::launch::async, [&a]() {
            std::vector<std::string> linhas;
            try
            {
                linhas.push_back(a.receiveLine()); // SORTEIO: ...
                linhas.push_back(a.receiveLine()); // APOSTA 1: ...
                linhas.push_back(a.receiveLine()); // APOSTA 2: ...
            }
            catch (const std::exception&) {}
            return linhas;
        });

        bool chegou = leitura.wait_for(std::chrono::seconds(80)) == std::future_status::ready;
        if (!chegou)
        {
            // destrava o receiveLine derrubando o servidor, senao o future nunca resolve
            TerminateProcess(servidor.hProcess, 0);
        }

        std::vector<std::string> linhas = leitura.get();
        check(chegou, "sorteio chegou dentro de 80s");

        if (linhas.size() >= 1)
        {
            check(linhas[0].rfind("SORTEIO: ", 0) == 0, "primeira linha e o sorteio");
            std::cout << "       " << linhas[0] << "\n";
        }
        if (linhas.size() >= 3)
        {
            check(linhas[1].rfind("APOSTA 1: ", 0) == 0, "segunda linha e a aposta 1");
            check(linhas[2].rfind("APOSTA 2: ", 0) == 0, "terceira linha e a aposta 2");
            std::cout << "       " << linhas[1] << "\n";
            std::cout << "       " << linhas[2] << "\n";
        }
        else
        {
            check(false, "as 2 apostas do cliente A foram conferidas");
        }

        // ---- 7. desconexao nao derruba o servidor ----
        std::cout << "[7] desconexao\n";
        a.close();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        Socket c;
        c.connectTo("127.0.0.1", PORTA);
        std::string welcomeC = c.receiveLine();
        check(welcomeC.substr(8) == ": CONECTADO!!", "servidor continua aceitando apos um cliente sair");
        checkIgual(pedir(c, "7"), "APOSTA REGISTRADA", "novo cliente comeca com loteria limpa");
    }
    catch (const std::exception& e)
    {
        std::cerr << "  EXCECAO no teste: " << e.what() << "\n";
        ++falhas;
    }

    TerminateProcess(servidor.hProcess, 0);
    CloseHandle(servidor.hProcess);
    CloseHandle(servidor.hThread);

    // so foi criado se a secao [5] chegou a rodar; sem isso ele ficaria orfao segurando a porta
    if (servidorLimite.hProcess != nullptr)
    {
        TerminateProcess(servidorLimite.hProcess, 0);
        CloseHandle(servidorLimite.hProcess);
        CloseHandle(servidorLimite.hThread);
    }

    std::cout << "\n"
              << (falhas == 0 ? "INTEGRACAO: TODOS OS TESTES PASSARAM"
                              : "INTEGRACAO: " + std::to_string(falhas) + " TESTE(S) FALHARAM")
              << "\n";
    return falhas == 0 ? 0 : 1;
}
