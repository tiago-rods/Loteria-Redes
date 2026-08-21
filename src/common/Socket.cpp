#include "Socket.hpp"
#include <ws2tcpip.h>
#include <stdexcept>

/*=======================
O que é: Construtor padrão
O que faz: Cria um novo socket TCP/IPv4 e guarda o handle no objeto
Como faz: Chama socket() do Winsock com AF_INET (IPv4), SOCK_STREAM (TCP) e IPPROTO_TCP; se falhar, lança exceção
Possíveis dúvidas: a falha na criação retorna INVALID_SOCKET (diferente de SOCKET_ERROR, usado pelas outras chamadas como bind/connect/send)
========================*/
Socket::Socket()
    : handle_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) // IPv4, TCP, lança a excessão se falhar
    {
        if (handle_ == INVALID_SOCKET)
        {
            throw std::runtime_error("erro na criação de socket");
        }
    }

/*=======================
O que é: Construtor que envolve um handle já existente
O que faz: Guarda um SOCKET criado em outro lugar (ex: retornado por accept())
Como faz: Apenas atribui o handle recebido à variável membro, sem chamar socket() de novo
Possíveis dúvidas: existe separado do construtor padrão porque accept() já devolve 
um handle pronto — não faz sentido criar outro socket do zero e depois trocar
========================*/
Socket::Socket(SOCKET handle)
    : handle_(handle) {}

/*=======================
O que é: Destrutor
O que faz: Garante que o socket seja fechado quando o objeto sai de escopo (RAII)
Como faz: Chama close()
Possíveis dúvidas: RAII = o recurso (socket) é liberado automaticamente pelo destrutor,
 então não precisamos lembrar de fechar manualmente em todo lugar
========================*/
Socket::~Socket()
{
    close(); // Destrutor
}

/*=======================
O que é: Construtor de movimento
O que faz: Transfere a posse do handle de outro Socket para este, sem duplicar a conexão
Como faz: Copia o handle do objeto "other" e zera o handle dele (INVALID_SOCKET),
 pra o destrutor do "other" não fechar o socket que acabou de ser transferido
Possíveis dúvidas: por que não copiar em vez de mover? Dois objetos com o mesmo
 handle tentariam fechar o mesmo socket no destrutor, causando erro (double close)
========================*/
Socket::Socket(Socket&& other)
    noexcept : handle_(other.handle_)
{
        other.handle_ = INVALID_SOCKET;
}

/*=======================
O que é: Operador de atribuição por movimento
O que faz: Substitui o socket atual pelo do outro objeto
Como faz: Fecha o socket atual (se houver), copia o handle do "other" e invalida o handle original
Possíveis dúvidas: o "if (this != &other)" evita fechar o próprio handle no caso raro de auto-atribuição (ex: obj = std::move(obj))
========================*/
Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other)
    {
        close();
        handle_ = other.handle_;
        other.handle_ = INVALID_SOCKET;
    }
    return *this;
}

/*=======================
O que é: Conecta o socket a um servidor
O que faz: Estabelece uma conexão TCP com o host e a porta informados
Como faz: Monta um sockaddr_in com o IP (convertido de string pra binário via inet_pton)
 e a porta (convertida pra network byte order via htons), depois chama connect()
Possíveis dúvidas: htons converte a porta do formato do computador (little-endian, geralmente) 
pro formato de rede (big-endian) — sem isso a porta chegaria errada do outro lado
========================*/
void Socket::connectTo(const std::string& host, unsigned short port)
{
/* apenas para saber quais dados guarda
struct sockaddr_in {
    short          sin_family;   // família de endereço: AF_INET (IPv4)
    u_short        sin_port;     // porta, em network byte order (por isso o htons)
    struct in_addr sin_addr;     // endereço IPv4 (32 bits), dentro tem o campo s_addr
    char           sin_zero[8];  // padding, não usado — só preenche o tamanho
};*/
    sockaddr_in addr{};  // inicializaçao por brace, zera todos os campos da struct
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    // reinterpter_cast -> como sockaddr_in* e sockaddr* são structs diferentes, não da para fazer um cast direto
    // logo é necessário explicitar que isso é seguro e o layout de memória é compatível, o que a documentação do winsock garante
    if(::connect(handle_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) //obs quando o :: é usado sem namespace antes, ele se refere ao namespace global, porém dá prioridade para o da mesma classe, ademais, como o header vem de um arquivo em C, que não tem namespace, tudo cai no namespace global
    {
        throw std::runtime_error("erro ao conectar");
    }
}

/*=======================
O que é: Associa o socket a uma porta local
O que faz: Reserva uma porta na máquina para o socket poder escutar conexões
Como faz: Preenche um sockaddr_in com INADDR_ANY (aceita conexões vindas de qualquer interface de rede da máquina)
 e a porta desejada, depois chama bind()
Possíveis dúvidas: só faz sentido no lado servidor — o cliente não precisa,
 o sistema operacional escolhe uma porta livre sozinho ao conectar
========================*/
void Socket::bindTo(unsigned short port)
{
    sockaddr_in addr{}; // inicializaçao por brace, zera todos os campos da struct
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if(::bind(handle_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        throw std::runtime_error("falha no bind");
    }
}

/*=======================
O que é: Coloca o socket em modo de escuta
O que faz: Sinaliza ao sistema operacional que este socket vai aceitar conexões de entrada
Como faz: Chama listen(), passando o tamanho da fila de conexões pendentes (backlog)
Possíveis dúvidas: backlog é quantas conexões podem esperar na fila antes de serem processadas por accept();
 SOMAXCONN deixa o SO escolher um valor razoável
========================*/
void Socket::listenFor(int backlog)
{
    if (::listen(handle_, backlog) == SOCKET_ERROR)
    {
        throw std::runtime_error("falha no listen");
    }
}

/*=======================
O que é: Aceita uma conexão de um cliente
O que faz: Bloqueia até um cliente se conectar, e devolve um novo Socket representando essa conexão
Como faz: Chama accept() do Winsock, que devolve um novo handle, e o envolve num Socket usando o construtor explicit(SOCKET)
Possíveis dúvidas: o socket original (o que fez bind/listen) continua livre pra aceitar outras conexões,
o handle retornado aqui é um socket separado, dedicado a esse cliente específico
========================*/
Socket Socket::accept()
{
    SOCKET client = ::accept(handle_, nullptr, nullptr);
    if (client == INVALID_SOCKET) 
    {
        throw std::runtime_error("falha ao aceitar conexão");
    }
    return Socket(client);
}

/*=======================
O que é: Envia todos os bytes de uma string
O que faz: Garante que a mensagem inteira seja enviada, mesmo que send() não mande tudo de uma vez
Como faz: Chama send() em loop, avançando pelos bytes já enviados, até totalSent alcançar o tamanho total dos dados
Possíveis dúvidas: por que não mandar tudo numa chamada só? send() pode enviar só uma parte dos dados de uma vez
 (short write), principalmente em mensagens grandes, por isso o loop
========================*/
void Socket::sendAll(const std::string& data)
{
    int totalSent = 0;
    int len = static_cast<int>(data.size());
    
    while(totalSent < len)
    {
        int sent = ::send(handle_, data.data() + totalSent, len - totalSent, 0);
        if (sent == SOCKET_ERROR)
        {
            throw std::runtime_error("falha ao enviar dados");
        }
        totalSent += sent;
    }
}

/*=======================
O que é: Lê uma linha de texto recebida
O que faz: Recebe bytes até encontrar um '\n', que marca o fim de uma mensagem
Como faz: Chama recv() um byte por vez, acumulando em "result", até achar '\n'
Possíveis dúvidas: TCP é um fluxo contínuo de bytes, sem separação nativa de mensagens,
 por isso o protocolo do projeto usa '\n' como delimitador, pra saber onde uma mensagem termina
========================*/
std::string Socket::receiveLine()
{
    std::string result;
    char c;
    while(1)
    {
        int received = ::recv(handle_, &c, 1, 0);
        if (received == 0)  
        {
        throw std::runtime_error("conexao fechada pelo peer");
        }
        if (received == SOCKET_ERROR)
        {
            throw std::runtime_error("falha ao receber dados");
        }
    if (c == '\n') break;
    result.push_back(c);
    }
return result;
}

/*=======================
O que é: Fecha o socket
O que faz: Libera o handle junto ao sistema operacional, se ainda estiver válido
Como faz: Chama closesocket() e marca o handle como INVALID_SOCKET
Possíveis dúvidas: por que checar handle_ != INVALID_SOCKET antes? Porque close()
 pode ser chamado mais de uma vez (ex: manualmente e depois pelo destrutor), e fechar um handle já fechado é erro
========================*/
void Socket::close()
{
    if (handle_ != INVALID_SOCKET) 
    {
        closesocket(handle_);
        handle_ = INVALID_SOCKET;
    }
}

/*=======================
O que é: Verifica se o socket é válido
O que faz: Informa se o objeto ainda representa um socket aberto
Como faz: Compara o handle com INVALID_SOCKET
Possíveis dúvidas: útil pra checar o estado depois de um move 
(o objeto "esvaziado" fica com handle_ == INVALID_SOCKET, logo isValid() retorna false)
========================*/
bool Socket::isValid() const
{
    return handle_ != INVALID_SOCKET;
}
