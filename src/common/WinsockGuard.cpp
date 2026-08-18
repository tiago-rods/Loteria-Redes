#include "WinsockGuard.hpp"
#include <stdexcept>

/*=======================
O que é: Construtor
O que faz: Inicializa a biblioteca Winsock para o processo inteiro
Como faz: Chama WSAStartup pedindo a versão 2.2 (MAKEWORD(2,2)); a struct WSADATA é
 preenchida com detalhes da implementação, mas não precisamos do conteúdo dela aqui
Possíveis dúvidas: diferente de socket()/bind()/connect(), que sinalizam erro retornando
 INVALID_SOCKET ou SOCKET_ERROR, o WSAStartup devolve o próprio código de erro diretamente
 (0 = sucesso, != 0 = código de erro)
========================*/
WinsockGuard::WinsockGuard()
{
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (result != 0) throw std::runtime_error("falha na inicialização do winsock");
}

/*=======================
O que é: Destrutor
O que faz: Libera os recursos usados pela biblioteca Winsock
Como faz: Chama WSACleanup()
Possíveis dúvidas: por que não checar o valor de retorno de WSACleanup? Porque destrutores
 não devem lançar exceções, e não haveria ação corretiva possível de qualquer forma
========================*/
WinsockGuard::~WinsockGuard()
{
    WSACleanup();
}