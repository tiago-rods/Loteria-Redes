#pragma once
#include<winsock2.h>

// RAII para o ciclo de vida da biblioteca Winsock inteira, não de um socket individual
// Deve existir exatamente UMA instância viva por processo, criada no início do main() antes de qualquer Socket ser construído
class WinsockGuard 
{
public: 
    WinsockGuard(); // chama WSAStartup, e lança exceção em caso de erro
    ~WinsockGuard(); // chamaWSAClenaup

    // Não faz sentido ter duas instãncias vivas, cada uma chamaria WSAClenaup no próprio destrutor, desligando o winsock enquanto a outra ainda acha que este está ativo
    WinsockGuard(const WinsockGuard&) = delete;
    WinsockGuard& operator=(const WinsockGuard&) = delete; // overload de operador
    
    // mesma lógica, não há um handle para esvaziar e transferir como no socket, e como só existe uma instância por processo, não há utilidade para move
    WinsockGuard(WinsockGuard&&) = delete; // obs: && é referência a rvalue, o std::move(x) transforma um lvalue em um rvalue
    WinsockGuard& operator=(WinsockGuard&&) = delete;
};