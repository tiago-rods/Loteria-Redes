# Coisas Que Devemos Estudar Para Fazer Esse Projeto
Vou dividir em alguns tópicos para ficar mais fácil para gente
Se quiserem ir estudando e anotando resumos aqui para todos entenderem, se não também suave
## Conceitos de C++
- estudar utilização de threads em C++ com `std::thread`
- estudar conceitos de mutex e lock_guard com `std::mutex` e `std::lock_guard`
- estudar lambdas 
- estudar parseamento de strings
- estudar move semantics pois sockets não devem ser copiados ao ser passados em fuções, apenas movidos

### lvalue vs rvalue
- **lvalue**: tem nome, tem endereço, "persiste" depois da expressão. Ex: uma variável comum (`Socket s1;` → `s1` é lvalue)
- **rvalue**: é temporário, sem nome, desaparece no fim da expressão. Ex: retorno de função (`server.accept()`), literais, ou o resultado de `std::move(x)`
- `std::move(x)` não move nada sozinho — só faz um cast que transforma um lvalue em algo que o compilador trata como rvalue. É uma promessa: "não vou mais usar esse objeto pelo nome antigo, pode saquear ele"

### `&` vs `&&`
- `Tipo&` = referência a **lvalue** (a referência "normal")
- `Tipo&&` = referência a **rvalue** — só se liga a temporários (ou a algo passado por `std::move`)
- É por isso que construtor/atribuição de movimento usam `&&`: eles só devem ser chamados quando o objeto de origem é descartável, nunca quando é um objeto que ainda vai ser usado depois

```cpp
void f(Socket& s);   // só aceita lvalues -> f(s1) ok, f(server.accept()) erro de compilação
void f(Socket&& s);  // só aceita rvalues -> f(s1) erro, f(server.accept()) ok, f(std::move(s1)) ok
```

### `operator=` é overload de operador
- Em C++, `operator=`, `operator+`, `operator==` etc. são funções com nome especial que o compilador chama quando você usa o símbolo. `a = b` vira `a.operator=(b)`
- Dá pra declarar várias versões de `operator=` com parâmetros diferentes; o compilador escolhe qual chamar baseado no tipo/categoria de valor do lado direito — decisão feita em **tempo de compilação**, não em runtime

```cpp
Socket& operator=(const Socket&) = delete; // chamado se o lado direito é lvalue -> mas está deletado, então nem compila
Socket& operator=(Socket&&) noexcept;      // chamado se o lado direito é rvalue

Socket a, b;
b = a;              // "a" é lvalue -> tentaria operator=(const Socket&) -> ERRO (deletado de propósito)
b = std::move(a);   // rvalue -> chama operator=(Socket&&), rouba o handle de "a"
b = Socket();        // Socket() é temporário (rvalue) -> também chama operator=(Socket&&)
```

- Cuidado: `operator=(Socket&&)` nunca deve ser `const` no parâmetro (`const Socket&&`) — a ideia do `&&` é justamente poder mutar/roubar o objeto de origem. `const T&&` compila mas geralmente é erro de digitação

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
