#include "Protocol.hpp"
#include<sstream> 
#include<ctime> 
#include<utility>

/*=======================
O que é: parser de comando (":inicio N", ":fim N", ":qtd N")
O que faz: identifica qual dos três comandos é e extrai o número associado
Como faz: usa istringstream para separar a palavra-chave do valor numérico
 (o operador >> pula espaços automaticamente e para no próximo espaço/fim), depois
 compara a palavra-chave lida com as três válidas
Possíveis dúvidas: static aqui restringe a visibilidade dessa função a este .cpp,
 já que é só um detalhe interno de como parseLine funciona, não precisa aparecer no .hpp
========================*/
// obs static significa que essa função só é visível dentro deste .cpp
static ProtocolMessage parseCommand(const std::string& line) // recebe a linha por const para evitar cópia
{
    std::istringstream iss(line); // Cria uma stream de entrada que é alimentada pelo conteúdo de line, permite usar o operador >> para ler token por token
    std::string keyword; // guarda a palavra chave do comando
    int value; // guarda o número associado a esse

    // o operador >> em stream pula os espaços automaticamente e para no próximo espaço/fim, logo ":inicio 5" vira
    // keyword = :inicio e value = 5, sem necessidade de parsing manual de indices
    iss >> keyword; // lê o primeiro token (tudo até o primeiro espaço) e guarda em keyword

    //tenta ler o próximo token e converter para int, guardando-o em value, retorna a propria stream que converte para bool dependendo da saída
    if (!(iss >> value)) return ProtocolMessage{}; // retorna invalido por padrão


    ProtocolMessage msg; // cria o objeto
    msg.value = value; // guarda o valor

    //guarda o tipo
    if(keyword == ":inicio") msg.type = ProtocolMessage::Type::SetInicio;
    else if(keyword == ":fim") msg.type = ProtocolMessage::Type::SetFim;
    else if(keyword == ":qtd") msg.type = ProtocolMessage::Type::SetQtd;
    else msg.type = ProtocolMessage::Type::Invalid; // talvez meio redundante pois já é por padrão mas decidi deixar

    return msg;
}

/*=======================
O que é: parser da lista de números de uma aposta ("N N N ...")
O que faz: extrai todos os números da linha, ou descarta a aposta inteira se achar lixo no meio
Como faz: lê token por token via istringstream, empurrando cada número pro vetor até a
 extração falhar; depois checa se falhou porque a string acabou (aposta válida) ou porque
 achou um token não numérico (aposta inválida)
Possíveis dúvidas: iss.eof() vs iss.fail() -> quando o while para, ou a string acabou
 (eof() true, todos os tokens eram números) ou apareceu algo que não é número no meio
 (fail() true e eof() ainda false nesse ponto) — é essa distinção que decide se a aposta
 é aceita ou rejeitada inteira, em vez de aceita parcialmente
========================*/
static ProtocolMessage parseAposta(const std::string& line)
{
    std::istringstream iss(line); // mesmo esquema de antes, lê token a token como o cin leria as teclas do teclado
    std::vector<int> numbers; // vetor numbers vai acumular os números extraídos da aposta.
    int n; // n é uma variável auxiliar onde cada número lido é temporariamente guardado antes de ir pro vetor

    // a cada iteração iss >> tenta ler o próximo token da stream, pular expaços em branco, converter para int e guardar em n
    // o operador >> retorna a própria iss. Uma istringstream pode converter para bool se a última extração deu certo, e false se ocorreu algum erro
    // ou, então o token não era um número. Logo o While roda enquanto continuar conseguindo extrair inteiros válidos. A cada sucesso, o número lido é empurrado para o vetor
    // exemplo de como funciona -> stream "2 42 66", 1° lê 2, 2° lê 42, 3° lê 66, e na 4° da eof logo para de rodar
    while (iss >> n) numbers.push_back(n);

    if(!iss.eof() && iss.fail()) return ProtocolMessage{}; // necessário ver o motivo pelo qual o while parou, se achou caracteres no meio da stream
    // ProtocolMessage tem por padrão tipo Type::Invalid
    if(numbers.empty()) return ProtocolMessage{}; // caso seja uma stream vazia

    // cria a classe de mensagem com o tipo correto e os numeros
    ProtocolMessage msg;
    msg.type = ProtocolMessage::Type::Aposta;
    msg.numbers = std::move(numbers); // usa move pois o vetor é caro para copiar
    return msg;
}


/*=======================
O que é: ponto de entrada do parser, decide entre comando e aposta
O que faz: identifica se a linha recebida é um comando (":...") ou uma aposta (números)
Como faz: olha apenas o primeiro caractere da linha e delega para parseCommand ou parseAposta
Possíveis dúvidas: por que checar line.empty() antes de acessar line[0]? acessar um índice
 de uma string vazia é comportamento indefinido, essa checagem evita isso e já retorna
 Invalid nesse caso
========================*/
ProtocolMessage parseLine(const std::string& line)
{
    if(line.empty()) return ProtocolMessage{}; // se não vier nada, retorna invalido

    if(line[0] == ':') return parseCommand(line); // olha o primeiro caractere, se for ":" faz o parse do comando

    return parseAposta(line); // se não começa com ":" o unico outro caso é aposta
}

/*=======================
O que é: monta a mensagem de boas-vindas enviada ao cliente ao conectar
O que faz: gera a string no formato "<HORARIO>: CONECTADO!!" usando o horário local atual
Como faz: pega o tempo atual com std::time, converte pra horário local decomposto (struct tm)
 com localtime_s, formata como "HH:MM:SS" com strftime, e concatena com o texto fixo
Possíveis dúvidas: por que localtime_s em vez de localtime? o localtime "clássico" da C
 devolve um ponteiro pra um buffer estático interno, não é thread-safe; como o projeto tem
 múltiplas threads rodando ao mesmo tempo, localtime_s evita isso preenchendo a struct
 recebida por parâmetro em vez de usar estado compartilhado
========================*/
std::string formatWelcomeMessage()
{
    std::time_t now = std::time(nullptr); //função que devolve o tempo atual
    char buf[9]; //HH:MM:SS
    std::tm localTime; // struct que tem campos separados para diferentes unidades de tempo
    localtime_s(&localTime, &now); // versão do windows de soltar o tempo local
    std::strftime(buf, sizeof(buf), "%H:%M:%S", &localTime);

    return std::string(buf) + ": CONECTADO!!";
}