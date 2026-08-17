# Coisas Que Devemos Estudar Para Fazer Esse Projeto
Vou dividir em alguns tópicos para ficar mais fácil para gente
Se quiserem ir estudando e anotando resumos aqui para todos entenderem, se não também suave
## Conceitos de C++
- estudar utilização de threads em C++ com `std::thread`
- estudar conceitos de mutex e lock_guard com `std::mutex` e `std::lock_guard`
- estudar lambdas 
- estudar parseamento de strings
- estudar move semantics pois sockets não devem ser copiados ao ser passados em fuções, apenas movidos

## API do Winsock
- WSAStartup
- socket
- bind
- listen
- accept
- connect
- send
- recv
- closesocket
- WSASClenup

## Encapsular com RAII
Pelo visto é boa prática refatorar a classe Socket + WinsockGuard para trabalhar com strings 
- Entender o que é uma classe RAII

## Threads
- Entender qual  deve ser a função de cada thread
- lembrar como funciona o join

## Servidor
- Accept loop 
- Não sei se precisa de vários clientes simultâneos, mas se precisar, devemos ver isso

## Parsing do protocolo
Implementar um parser dos comandos do trabalho
:inicio N
:fim N
:qtd N

## Estado da loteria (cpa que vai precisar de mutex)
Não sei direito o que fazer aqui ainda
