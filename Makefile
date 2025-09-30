# Makefile para o projeto de Estacionamento

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread -D_GNU_SOURCE -I./config -I./src/common
LDFLAGS = -lm -lpigpio -lrt -lpthread
LDFLAGS_FULL = -lmodbus -levent -lm -lpigpio -lrt -lpthread

# Diretórios
SRC_DIR = src
BUILD_DIR = build
COMMON_DIR = $(SRC_DIR)/common
TEST_DIR = tests

# Common source files (modo normal ou mock)
ifeq ($(MOCK),1)
	CFLAGS += -DMOCK_BUILD
	COMMON_SOURCES = $(COMMON_DIR)/gpio_control_mock.c \
									 $(COMMON_DIR)/modbus_client_mock.c \
									 $(COMMON_DIR)/tcp_communication_mock.c \
									 $(COMMON_DIR)/parking_logic.c \
									 $(COMMON_DIR)/gate_control.c \
									 $(COMMON_DIR)/system_logger.c
	LDFLAGS_USED = -lm -lpthread
else
	COMMON_SOURCES = $(COMMON_DIR)/gpio_control.c \
									 $(COMMON_DIR)/modbus_client.c \
									 $(COMMON_DIR)/tcp_communication.c \
									 $(COMMON_DIR)/parking_logic.c \
									 $(COMMON_DIR)/gate_control.c \
									 $(COMMON_DIR)/system_logger.c
	LDFLAGS_USED = $(LDFLAGS_FULL)
endif

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Main executables
all: $(BUILD_DIR) servidor_central servidor_terreo servidor_andar1 servidor_andar2 test_modbus

# Servidor Central
servidor_central: $(BUILD_DIR)/servidor_central
$(BUILD_DIR)/servidor_central: $(BUILD_DIR) $(SRC_DIR)/servidor_central/main.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_central/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)

# Servidor Térreo
servidor_terreo: $(BUILD_DIR)/servidor_terreo
$(BUILD_DIR)/servidor_terreo: $(BUILD_DIR) $(SRC_DIR)/servidor_terreo/main.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_terreo/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)

# Servidor 1º Andar
servidor_andar1: $(BUILD_DIR)/servidor_andar1
$(BUILD_DIR)/servidor_andar1: $(BUILD_DIR) $(SRC_DIR)/servidor_andar1/main.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_andar1/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)

# Servidor 2º Andar
servidor_andar2: $(BUILD_DIR)/servidor_andar2
$(BUILD_DIR)/servidor_andar2: $(BUILD_DIR) $(SRC_DIR)/servidor_andar2/main.c $(COMMON_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SRC_DIR)/servidor_andar2/main.c $(COMMON_SOURCES) $(LDFLAGS_USED)

# Teste MODBUS
# test_modbus: $(BUILD_DIR)/test_modbus
# $(BUILD_DIR)/test_modbus: $(BUILD_DIR) tests/test_modbus.c $(COMMON_SOURCES)
# 	$(CC) $(CFLAGS) -o $@ tests/test_modbus.c $(COMMON_SOURCES) $(LDFLAGS_USED)

# Clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f logs/*.log

# Help
help:
	@echo "Makefile para Sistema de Controle de Estacionamento"
	@echo ""
	@echo "Targets disponíveis:"
	@echo "  all              - Compila todos os servidores e testes"
	@echo "  servidor_central - Compila o servidor central"
	@echo "  servidor_terreo  - Compila o servidor do térreo"
	@echo "  servidor_andar1  - Compila o servidor do 1º andar"
	@echo "  servidor_andar2  - Compila o servidor do 2º andar"
	@echo "  test_modbus      - Compila teste do cliente MODBUS"
	@echo "  (use MOCK=1 para compilar sem libs externas: ex: make servidor_central MOCK=1)"
	@echo "  clean            - Remove arquivos compilados"
	@echo "  help             - Exibe esta mensagem"

.PHONY: all clean help servidor_central servidor_terreo servidor_andar1 servidor_andar2 test_modbus