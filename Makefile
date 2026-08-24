# Makefile do Loteria-Redes
#
# Alvos principais:
#   make            -> compila e roda os testes da Lottery (padrao)
#   make all        -> compila cliente e servidor
#   make client     -> so o cliente   -> bin/client.exe
#   make server     -> so o servidor  -> bin/server.exe
#   make test       -> compila e roda tests/test_lottery.exe
#   make run-server -> compila e executa o servidor
#   make run-client -> compila e executa o cliente
#   make clean      -> apaga build/ e bin/
#
# ATENCAO: as receitas rodam no cmd.exe, nao no bash. O make ate diz "SHELL=sh.exe",
# mas isso e so o valor padrao dele: como nao existe sh.exe no PATH, ele cai pro cmd.
# Por isso as receitas usam "if not exist ... mkdir" e "rmdir /S /Q" em vez de
# "mkdir -p" e "rm -rf", e barra invertida nos caminhos passados pro cmd.
# (Nao adicione C:\msys64\usr\bin no PATH so pra ter o sh: aquilo sobrescreve o
# find, o sort e o timeout do Windows e quebra outras coisas.)

CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -g -MMD -MP
LDLIBS   := -lws2_32

BUILD := build
BIN   := bin

# ---- objetos -------------------------------------------------------------
# Socket, WinsockGuard e Protocol sao usados pelos dois lados
COMMON_OBJ := $(BUILD)/Socket.o $(BUILD)/WinsockGuard.o $(BUILD)/Protocol.o

CLIENT_OBJ := $(COMMON_OBJ) $(BUILD)/Client.o $(BUILD)/client_main.o

SERVER_OBJ := $(COMMON_OBJ) $(BUILD)/Lottery.o $(BUILD)/Server.o \
              $(BUILD)/ClientSession.o $(BUILD)/server_main.o

# O teste de unidade NAO linka com winsock: a Lottery e pura regra de jogo, nao inclui socket
TEST_OBJ := $(BUILD)/Lottery.o $(BUILD)/test_lottery.o

# O de integracao sobe o server.exe de verdade e conversa com ele por TCP, entao precisa
# do Socket (e do winsock junto)
E2E_OBJ := $(COMMON_OBJ) $(BUILD)/test_integration.o

ALL_OBJ := $(sort $(CLIENT_OBJ) $(SERVER_OBJ) $(TEST_OBJ) $(E2E_OBJ))

CLIENT_EXE := $(BIN)/client.exe
SERVER_EXE := $(BIN)/server.exe
TEST_EXE   := $(BIN)/test_lottery.exe
E2E_EXE    := $(BIN)/test_integration.exe

.DEFAULT_GOAL := test

# src/client/main.cpp e src/server/main.cpp ainda estao vazios. Sem uma funcao main(),
# o linker do MinGW cospe "undefined reference to `WinMain'", que nao ajuda nada a
# entender o problema. As checagens abaixo trocam isso por uma mensagem clara.
# $(file <arquivo) le o conteudo do arquivo na hora que o make interpreta o Makefile.
CLIENT_MAIN_VAZIO := $(if $(strip $(file <src/client/main.cpp)),,sim)
SERVER_MAIN_VAZIO := $(if $(strip $(file <src/server/main.cpp)),,sim)

# ---- alvos ---------------------------------------------------------------
.PHONY: all client server test test-e2e test-all run-server run-client clean help

all: client server

ifeq ($(CLIENT_MAIN_VAZIO),sim)
client:
	@echo ERRO: src/client/main.cpp esta vazio - falta a funcao main^(^).
	@echo O cliente so linka depois que ela existir ^(criar Client, chamar connect^(^) e run^(^)^).
	@exit 1
else
client: $(CLIENT_EXE)
endif

ifeq ($(SERVER_MAIN_VAZIO),sim)
server:
	@echo ERRO: src/server/main.cpp esta vazio - falta a funcao main^(^).
	@echo Faltam tambem Server.cpp e ClientSession.cpp, que ainda estao vazios.
	@exit 1
else
server: $(SERVER_EXE)
endif

test: $(TEST_EXE)
	@echo === testes de unidade da Lottery ===
	@$(TEST_EXE)

# Precisa do server.exe compilado: o teste sobe o binario de verdade
test-e2e: $(SERVER_EXE) $(E2E_EXE)
	@echo === teste de integracao ponta a ponta (demora ~65s) ===
	@$(E2E_EXE)

test-all: test test-e2e
	@echo === tudo verde ===

run-server: $(SERVER_EXE)
	@$(SERVER_EXE)

run-client: $(CLIENT_EXE)
	@$(CLIENT_EXE)

# ---- links ---------------------------------------------------------------
$(CLIENT_EXE): $(CLIENT_OBJ) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(SERVER_EXE): $(SERVER_OBJ) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

$(TEST_EXE): $(TEST_OBJ) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(E2E_EXE): $(E2E_OBJ) | $(BIN)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDLIBS)

# ---- compilacao ----------------------------------------------------------
# Os dois main.cpp (cliente e servidor) tem o mesmo nome de arquivo, entao
# precisam de regras explicitas para nao colidirem no mesmo build/main.o
$(BUILD)/client_main.o: src/client/main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/server_main.o: src/server/main.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: src/common/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: src/client/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: src/server/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: tests/%.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ---- diretorios ----------------------------------------------------------
# Sao pre-requisitos "order-only" (depois do |): o make so garante que existem,
# sem reconstruir tudo toda vez que a data da pasta muda
$(BUILD):
	@if not exist "$(BUILD)" mkdir "$(BUILD)"

$(BIN):
	@if not exist "$(BIN)" mkdir "$(BIN)"

clean:
	@if exist "$(BUILD)" rmdir /S /Q "$(BUILD)"
	@if exist "$(BIN)" rmdir /S /Q "$(BIN)"
	@echo limpo.

help:
	@echo make            - compila e roda os testes (padrao)
	@echo make all        - compila cliente e servidor
	@echo make client     - so o cliente
	@echo make server     - so o servidor
	@echo make test       - testes de unidade da Lottery (rapido)
	@echo make test-e2e   - teste de integracao com o servidor real (~65s)
	@echo make test-all   - roda os dois
	@echo make run-server - executa o servidor
	@echo make run-client - executa o cliente
	@echo make clean      - apaga build/ e bin/

# Regenera automaticamente quando um .hpp muda (arquivos .d criados pelo -MMD -MP)
-include $(ALL_OBJ:.o=.d)
