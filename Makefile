# Makefile para o projeto de Estacionamento

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -D_GNU_SOURCE -I./config -I./src/common
LDFLAGS = -lm -lpthread -lrt

# Bibliotecas específicas para modo normal
LDFLAGS_PIGPIO = -lpigpio
LDFLAGS_MODBUS = -lmodbus
LDFLAGS_EVENT = -levent

# Diretórios
SRC_DIR = src
BUILD_DIR = build
COMMON_DIR = $(SRC_DIR)/common
CONFIG_DIR = config

# Detectar modo de compilação
ifeq ($(MOCK),1)
	CFLAGS += -DMOCK_BUILD
	MODE_SUFFIX = _mock
	COMMON_SOURCES = $(COMMON_DIR)/gpio_control_mock.c \
									 $(COMMON_DIR)/modbus_client_mock.c \
									 $(COMMON_DIR)/tcp_communication_mock.c \
									 $(COMMON_DIR)/parking_logic.c \
									 $(COMMON_DIR)/gate_control.c \
									 $(COMMON_DIR)/system_logger.c
	LDFLAGS_USED = $(LDFLAGS)
else
	MODE_SUFFIX =
	COMMON_SOURCES = $(COMMON_DIR)/gpio_control.c \
									 $(COMMON_DIR)/modbus_client.c \
									 $(COMMON_DIR)/tcp_communication.c \
									 $(COMMON_DIR)/parking_logic.c \
									 $(COMMON_DIR)/gate_control.c \
									 $(COMMON_DIR)/system_logger.c
	LDFLAGS_USED = $(LDFLAGS) $(LDFLAGS_PIGPIO) $(LDFLAGS_MODBUS) $(LDFLAGS_EVENT)
endif

# Criar diretório de build
$(BUILD_DIR):
	@echo "Criando diretório de build..."
	@mkdir -p $(BUILD_DIR)
	@mkdir -p logs

# Target padrão
.DEFAULT_GOAL := all

# Compilar todos os executáveis
all: check-deps $(BUILD_DIR) servidor_central servidor_terreo servidor_andar1 servidor_andar2
	@echo ""
	@echo "════════════════════════════════════════════════════════════"
	@echo "   Compilação concluída com sucesso!"
	@echo "════════════════════════════════════════════════════════════"
	@echo "Executáveis gerados em: $(BUILD_DIR)/"
	@echo ""
	@echo "Para executar:"
	@echo "  - Servidor Central:  $(BUILD_DIR)/servidor_central"
	@echo "  - Servidor Térreo:   $(BUILD_DIR)/servidor_terreo"
	@echo "  - Servidor 1º Andar: $(BUILD_DIR)/servidor_andar1"
	@echo "  - Servidor 2º Andar: $(BUILD_DIR)/servidor_andar2"
	@echo ""

# Verificar dependências (apenas modo normal)
check-deps:
ifneq ($(MOCK),1)
	@echo "Verificando dependências..."
	@echo -n "  - pigpio: "
	@pkg-config --exists pigpio 2>/dev/null && echo "OK" || (echo "NÃO ENCONTRADO - instale com: sudo apt-get install libpigpio-dev" && exit 1)
	@echo -n "  - libmodbus: "
	@pkg-config --exists libmodbus 2>/dev/null && echo "OK" || (echo "NÃO ENCONTRADO - instale com: sudo apt-get install libmodbus-dev" && exit 1)
	@echo -n "  - libevent: "
	@pkg-config --exists libevent 2>/dev/null && echo "OK" || (echo "NÃO ENCONTRADO - instale com: sudo apt-get install libevent-dev" && exit 1)
	@echo ""
else
	@echo "Modo MOCK ativado - compilando sem dependências externas"
	@echo ""
endif

# Servidor Central
servidor_central: $(BUILD_DIR)/servidor_central$(MODE_SUFFIX)
$(BUILD_DIR)/servidor_central$(MODE_SUFFIX): $(BUILD_DIR) $(SRC_DIR)/servidor_central/main.c $(COMMON_SOURCES)
	@echo "Compilando Servidor Central..."
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_central/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)
	@echo "  ✓ Servidor Central compilado"

# Servidor Térreo
servidor_terreo: $(BUILD_DIR)/servidor_terreo$(MODE_SUFFIX)
$(BUILD_DIR)/servidor_terreo$(MODE_SUFFIX): $(BUILD_DIR) $(SRC_DIR)/servidor_terreo/main.c $(COMMON_SOURCES)
	@echo "Compilando Servidor Térreo..."
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_terreo/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)
	@echo "  ✓ Servidor Térreo compilado"

# Servidor 1º Andar
servidor_andar1: $(BUILD_DIR)/servidor_andar1$(MODE_SUFFIX)
$(BUILD_DIR)/servidor_andar1$(MODE_SUFFIX): $(BUILD_DIR) $(SRC_DIR)/servidor_andar1/main.c $(COMMON_SOURCES)
	@echo "Compilando Servidor 1º Andar..."
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_andar1/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)
	@echo "  ✓ Servidor 1º Andar compilado"

# Servidor 2º Andar
servidor_andar2: $(BUILD_DIR)/servidor_andar2$(MODE_SUFFIX)
$(BUILD_DIR)/servidor_andar2$(MODE_SUFFIX): $(BUILD_DIR) $(SRC_DIR)/servidor_andar2/main.c $(COMMON_SOURCES)
	@echo "Compilando Servidor 2º Andar..."
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_andar2/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)
	@echo "  ✓ Servidor 2º Andar compilado"

# Limpeza
clean:
	@echo "Removendo arquivos compilados..."
	@rm -rf $(BUILD_DIR)
	@echo "  ✓ Diretório build removido"

clean-logs:
	@echo "Removendo logs..."
	@rm -f logs/*.log
	@echo "  ✓ Logs removidos"

clean-all: clean clean-logs
	@echo "Limpeza completa realizada"

# Instalar dependências (Ubuntu/Debian)
install-deps:
	@echo "Instalando dependências do sistema..."
	sudo apt-get update
	sudo apt-get install -y build-essential libpigpio-dev libmodbus-dev libevent-dev pkg-config
	@echo ""
	@echo "✓ Dependências instaladas com sucesso!"
	@echo ""

# Executar em modo desenvolvimento (com logs detalhados)
run-central: servidor_central
	@echo "Iniciando Servidor Central..."
	sudo $(BUILD_DIR)/servidor_central$(MODE_SUFFIX)

run-terreo: servidor_terreo
	@echo "Iniciando Servidor Térreo..."
	sudo $(BUILD_DIR)/servidor_terreo$(MODE_SUFFIX)

run-andar1: servidor_andar1
	@echo "Iniciando Servidor 1º Andar..."
	sudo $(BUILD_DIR)/servidor_andar1$(MODE_SUFFIX)

run-andar2: servidor_andar2
	@echo "Iniciando Servidor 2º Andar..."
	sudo $(BUILD_DIR)/servidor_andar2$(MODE_SUFFIX)

# Executar todos os servidores em terminais separados (requer tmux)
run-all: all
	@echo "Iniciando todos os servidores em sessão tmux..."
	@command -v tmux >/dev/null 2>&1 || (echo "tmux não encontrado. Instale com: sudo apt-get install tmux" && exit 1)
	tmux new-session -d -s parking 'sudo $(BUILD_DIR)/servidor_central$(MODE_SUFFIX); read'
	tmux split-window -h -t parking 'sudo $(BUILD_DIR)/servidor_terreo$(MODE_SUFFIX); read'
	tmux split-window -v -t parking 'sudo $(BUILD_DIR)/servidor_andar1$(MODE_SUFFIX); read'
	tmux split-window -v -t parking:0.0 'sudo $(BUILD_DIR)/servidor_andar2$(MODE_SUFFIX); read'
	tmux select-layout -t parking tiled
	tmux attach -t parking

# Stop todos os processos
stop-all:
	@echo "Parando todos os servidores..."
	@sudo pkill -f servidor_central || true
	@sudo pkill -f servidor_terreo || true
	@sudo pkill -f servidor_andar1 || true
	@sudo pkill -f servidor_andar2 || true
	@echo "  ✓ Servidores parados"

# Teste de compilação rápido
test-build: clean all
	@echo ""
	@echo "✓ Teste de compilação concluído com sucesso!"

# Help
help:
	@echo "════════════════════════════════════════════════════════════"
	@echo "   Makefile - Sistema de Controle de Estacionamento"
	@echo "════════════════════════════════════════════════════════════"
	@echo ""
	@echo "Targets de Compilação:"
	@echo "  all              - Compila todos os servidores"
	@echo "  servidor_central - Compila apenas o servidor central"
	@echo "  servidor_terreo  - Compila apenas o servidor do térreo"
	@echo "  servidor_andar1  - Compila apenas o servidor do 1º andar"
	@echo "  servidor_andar2  - Compila apenas o servidor do 2º andar"
	@echo ""
	@echo "Modo MOCK (sem hardware):"
	@echo "  make MOCK=1                - Compila em modo simulação"
	@echo "  make MOCK=1 servidor_central - Compila apenas central em MOCK"
	@echo ""
	@echo "Execução:"
	@echo "  make run-central - Executa servidor central"
	@echo "  make run-terreo  - Executa servidor térreo"
	@echo "  make run-andar1  - Executa servidor 1º andar"
	@echo "  make run-andar2  - Executa servidor 2º andar"
	@echo "  make run-all     - Executa todos em tmux"
	@echo "  make stop-all    - Para todos os servidores"
	@echo ""
	@echo "Limpeza:"
	@echo "  make clean       - Remove arquivos compilados"
	@echo "  make clean-logs  - Remove arquivos de log"
	@echo "  make clean-all   - Limpeza completa"
	@echo ""
	@echo "Utilitários:"
	@echo "  make install-deps - Instala dependências do sistema"
	@echo "  make check-deps   - Verifica dependências"
	@echo "  make test-build   - Teste de compilação completo"
	@echo "  make help         - Exibe esta mensagem"
	@echo ""
	@echo "════════════════════════════════════════════════════════════"

.PHONY: all clean clean-logs clean-all install-deps check-deps help \
        run-central run-terreo run-andar1 run-andar2 run-all stop-all \
        test-build servidor_central servidor_terreo servidor_andar1 servidor_andar2