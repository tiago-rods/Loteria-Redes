#include"Server.hpp"
#include"../common/WinsockGuard.hpp"
#include<iostream>
#include<string>    // std::string, std::stoi
#include<stdexcept> // std::runtime_error

/*=======================
O que é: Ponto de entrada do servidor
O que faz: Lê a porta (opcional) da linha de comando, inicializa o Winsock e sobe o Server
Como faz: o WinsockGuard é criado antes de qualquer Socket e só é destruído no fim do main,
 então o Winsock fica ativo durante toda a execução
Possíveis dúvidas: por que o try/catch em volta de tudo? Porque Socket e Server sinalizam erro
 por exceção (porta ocupada, falha no bind). Sem ele o programa morreria com a mensagem crua do
 runtime; assim o usuário lê o motivo e o processo devolve 1 para o sistema
========================*/
int main(int argc, char* argv[])
{
    unsigned short port = 54000; // mesma porta padrão do cliente

    try
    {
        // porta opcional por argv, ex: server.exe 54000
        if (argc >= 2)
        {
            int portValue;
            try
            {
                portValue = std::stoi(argv[1]); // lança invalid_argument/out_of_range se não for número
            }
            catch (const std::exception&)
            {
                throw std::runtime_error("porta invalida: '" + std::string(argv[1]) + "' nao e um numero");
            }

            // o 0 é recusado de propósito: ao escutar, ele faz o SO escolher uma porta
            // qualquer, e aí o cliente não teria como adivinhar em qual conectar
            if (portValue < 1 || portValue > 65535)
            {
                throw std::runtime_error("porta invalida, deve estar entre 1 e 65535");
            }
            port = static_cast<unsigned short>(portValue);
        }

        WinsockGuard guard; // inicia o Winsock; tem que vir antes de qualquer Socket

        Server server(port); // o construtor já faz bind e listen, lança se a porta estiver ocupada
        server.run();        // laço de accept: só retorna quando o listener falha ou fecha
    }
    catch(const std::exception& e)
    {
        std::cerr << "Erro: " << e.what() << std::endl; // explica o motivo e retorna 1
        return 1;
    }
    return 0;
}
