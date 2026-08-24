#include "ClientSession.hpp"
#include "../common/Protocol.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

/*=======================
O que é: Construtor da sessão de um cliente
O que faz: Guarda dentro de ClientSession o socket já conectado que veio do accept()
Como faz: Usa std::move para transferir a posse do Socket recebido para socket_
Possíveis dúvidas: não chamamos Socket() para criar uma nova conexão. O socket recebido
 já representa a conexão criada por accept(), então apenas transferimos esse recurso
 para dentro da sessão
========================*/
ClientSession::ClientSession(Socket socket)
    : socket_(std::move(socket))
{
}

/*=======================
O que é: Função auxiliar para envio de mensagens ao cliente
O que faz: Envia uma linha pelo socket adicionando '\n' como delimitador do protocolo
Como faz: Primeiro trava sendMutex_ com lock_guard, garantindo que apenas uma thread
 esteja enviando naquele momento, e depois chama sendAll()
Possíveis dúvidas: por que colocar o '\n' aqui? Porque receiveLine() do outro lado lê
 bytes até encontrar '\n'. Centralizar isso aqui também evita esquecer o delimitador
 em algum dos vários pontos onde o servidor envia mensagens
========================*/
void ClientSession::sendLine(const std::string& line)
{
    std::lock_guard<std::mutex> lock(sendMutex_);

    socket_.sendAll(line + "\n");
}

/*=======================
O que é: Método que inicia a sessão do cliente
O que faz: Envia a mensagem inicial de conexão, cria as duas threads exigidas pelo
 enunciado e espera até que ambas terminem
Como faz: receiveThread executa receiveLoop(), enquanto drawThread executa drawLoop().
 join() impede que run() termine enquanto as threads ainda estiverem executando
Possíveis dúvidas: o mesmo objeto ClientSession é usado pelas duas threads. Por isso
 passamos "this" para std::thread
========================*/
void ClientSession::run()
{
    // MSG1: "<HORARIO>: CONECTADO!!"
    sendLine(formatWelcomeMessage());

    std::thread receiveThread(&ClientSession::receiveLoop, this);
    std::thread drawThread(&ClientSession::drawLoop, this);

    receiveThread.join();
    drawThread.join();
}

/*=======================
O que é: Thread 1 do servidor - recepção de dados do cliente
O que faz: Fica aguardando comandos ou apostas enviados pelo cliente, interpreta a
 mensagem e executa a operação correspondente na loteria
Como faz: receiveLine() recebe uma linha, parseLine() transforma o texto em um
 ProtocolMessage e o switch verifica o Type da mensagem para decidir qual método
 de Lottery deve ser chamado
Possíveis dúvidas: receiveLine() é bloqueante, portanto enquanto nenhum dado chega
 do cliente esta thread fica parada aqui sem impedir a Thread 2 de continuar executando.
 Esse é justamente um dos motivos para existirem duas threads
========================*/
void ClientSession::receiveLoop()
{
    while (running_)
    {
        try
        {
            // bloqueia esta thread até chegar uma linha completa do cliente
            std::string line = socket_.receiveLine();

            // transforma o texto recebido em uma mensagem do protocolo
            ProtocolMessage msg = parseLine(line);

            switch (msg.type)
            {
                case ProtocolMessage::Type::SetInicio:
                {
                    bool sucesso;

                    {
                        std::lock_guard<std::mutex> lock(lotteryMutex_);
                        sucesso = lottery_.setInicio(msg.value);
                    }

                    if (sucesso)
                    {
                        sendLine(
                            "INICIO CONFIGURADO: "
                            + std::to_string(msg.value)
                        );
                    }
                    else
                    {
                        sendLine("ERRO: INICIO INVALIDO");
                    }

                    break;
                }

                case ProtocolMessage::Type::SetFim:
                {
                    bool sucesso;

                    {
                        std::lock_guard<std::mutex> lock(lotteryMutex_);
                        sucesso = lottery_.setFim(msg.value);
                    }

                    if (sucesso)
                    {
                        sendLine(
                            "FIM CONFIGURADO: "
                            + std::to_string(msg.value)
                        );
                    }
                    else
                    {
                        sendLine("ERRO: FIM INVALIDO");
                    }

                    break;
                }

                case ProtocolMessage::Type::SetQtd:
                {
                    bool sucesso;

                    {
                        std::lock_guard<std::mutex> lock(lotteryMutex_);
                        sucesso = lottery_.setQtd(msg.value);
                    }

                    if (sucesso)
                    {
                        sendLine(
                            "QUANTIDADE CONFIGURADA: "
                            + std::to_string(msg.value)
                        );
                    }
                    else
                    {
                        sendLine("ERRO: QUANTIDADE INVALIDA");
                    }

                    break;
                }

                case ProtocolMessage::Type::Aposta:
                {
                    bool sucesso;

                    {
                        std::lock_guard<std::mutex> lock(lotteryMutex_);
                        sucesso = lottery_.addAposta(msg.numbers);
                    }

                    if (sucesso)
                    {
                        sendLine("APOSTA REGISTRADA");
                    }
                    else
                    {
                        sendLine("ERRO: APOSTA INVALIDA");
                    }

                    break;
                }

                case ProtocolMessage::Type::Invalid:
                default:
                {
                    sendLine("ERRO: COMANDO OU APOSTA INVALIDA");
                    break;
                }
            }
        }
        catch (const std::exception& e)
        {
            // receiveLine() chega aqui principalmente quando o cliente fecha a conexão
            std::cerr
                << "Cliente desconectado: "
                << e.what()
                << std::endl;

            // avisa a Thread 2 que a sessão deve ser encerrada
            running_ = false;

            break;
        }
    }
}

/*=======================
O que é: Thread 2 do servidor - sorteio periódico
O que faz: Aguarda um minuto, realiza o sorteio, verifica as apostas e envia o resultado
 para o cliente. Depois começa um novo ciclo
Como faz: sleep_for() controla o intervalo; Lottery::draw() realiza o sorteio e limpa
 as apostas do ciclo; formatDrawResult() transforma o resultado em linhas de texto,
 que são enviadas individualmente com sendLine()
Possíveis dúvidas: por que o sleep foi dividido em períodos de 1 segundo em vez de fazer
 diretamente sleep_for(minutes(1))? Se o cliente desconectar logo depois do início do
 minuto, uma única espera de 60 segundos faria esta thread demorar quase um minuto para
 perceber running_ == false. Verificando uma vez por segundo, ela encerra rapidamente
 após a desconexão
========================*/
void ClientSession::drawLoop()
{
    while (running_)
    {
        // aguarda 60 segundos, verificando a conexão a cada segundo
        for (int i = 0; i < 60 && running_; ++i)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // se a Thread 1 detectou desconexão, encerra esta thread
        if (!running_)
        {
            break;
        }

        try
        {
            /*
            Lottery também é usada pela Thread 1.
            Por isso o mutex permanece travado somente durante o sorteio.
            */
            std::unique_lock<std::mutex> lock(lotteryMutex_);

            DrawResult result = lottery_.draw();

            lock.unlock();

            // transforma o resultado do sorteio em linhas do protocolo
            std::vector<std::string> lines = formatDrawResult(result);

            for (const std::string& line : lines)
            {
                sendLine(line);
            }
        }
        catch (const std::exception& e)
        {
            std::cerr
                << "Erro durante o sorteio/envio: "
                << e.what()
                << std::endl;

            running_ = false;

            break;
        }
    }
}