#pragma once
#include<string>
#include<vector>

// A classe de protocol tem o intuíto de parsear os comandos
// exemplo ":inicio 5", ":fim 100" e etc


class ProtocolMessage // representa uma linha já interpretada, recebida via Socket::receiveLine()
{
public: 
    enum class Type 
    {
        SetInicio, // :inicio
        SetFim, // :fim N
        SetQtd, // :qtd N
        Aposta, // N N N...
        Invalid // linha que não fica de acordo com o esperado
    };

    Type type = Type::Invalid; // Invalid por padrão
    int value = 0;
    std::vector<int> numbers;
};

ProtocolMessage parseLine(const std::string& line);

std::string formatWelcomeMessage();

