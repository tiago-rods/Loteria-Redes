#ifndef CLIENT_SESSION_HPP
#define CLIENT_SESSION_HPP

#include "../common/Socket.hpp"
#include "Lottery.hpp"

#include <atomic>
#include <mutex>
#include <string>

/*=======================
O que é: Classe que representa a sessão de um cliente conectado ao servidor
O que faz: Mantém o socket daquele cliente e coordena as duas threads exigidas pelo projeto:
 uma para receber comandos/apostas e outra para realizar os sorteios periodicamente
Como faz: Guarda o Socket recebido pelo accept(), uma Lottery com o estado das apostas e
 uma flag atômica que informa se a conexão ainda está ativa
Possíveis dúvidas: cada ClientSession representa uma conexão específica. O Server possui
 o socket que escuta novas conexões, enquanto ClientSession recebe o novo socket devolvido
 por accept(), que é dedicado à comunicação com aquele cliente
========================*/
class ClientSession
{
public:

    /*=======================
    O que é: Construtor da sessão
    O que faz: Recebe o socket conectado ao cliente e transfere sua posse para ClientSession
    Como faz: Recebe Socket por valor e posteriormente usa std::move na implementação
    Possíveis dúvidas: Socket não deve ser copiado porque dois objetos com o mesmo handle
     tentariam fechar a mesma conexão. Por isso a posse do socket é movida
    ========================*/
    explicit ClientSession(Socket socket);

    /*=======================
    O que é: Método principal da sessão
    O que faz: Envia a mensagem inicial, cria as duas threads do servidor e aguarda
     as duas terminarem
    Como faz: Cria std::thread apontando para receiveLoop() e drawLoop(), depois chama join()
    ========================*/
    void run();

private:

    Socket socket_;       // socket dedicado a este cliente
    Lottery lottery_;     // configuração, apostas e sorteio deste cliente

    // atomic permite que as duas threads leiam/escrevam esta variável sem data race
    std::atomic<bool> running_{true};

    // protege o envio pelo socket, pois as duas threads podem tentar mandar mensagens
    // ao cliente ao mesmo tempo
    std::mutex sendMutex_;

    /*=======================
    O que é: Thread 1 do servidor
    O que faz: Aguarda mensagens do cliente e interpreta comandos ou apostas
    Como faz: Usa receiveLine(), parseLine() e chama os métodos correspondentes de Lottery
    ========================*/
    void receiveLoop();

    /*=======================
    O que é: Thread 2 do servidor
    O que faz: Aguarda o intervalo do sorteio, realiza o sorteio e envia o resultado
     ao cliente
    Como faz: Espera periodicamente usando sleep_for(), chama Lottery::draw(),
     formatDrawResult() e envia cada linha
    ========================*/
    void drawLoop();

    /*=======================
    O que é: Função auxiliar de envio
    O que faz: Envia uma linha ao cliente garantindo que apenas uma thread escreva
     no socket por vez
    Como faz: Usa lock_guard no sendMutex_ e chama Socket::sendAll()
    ========================*/
    void sendLine(const std::string& line);
};

#endif