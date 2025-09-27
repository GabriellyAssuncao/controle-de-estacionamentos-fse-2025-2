/**
 * @file modbus_client.c
 * @brief Implementação do cliente MODBUS RTU usando libmodbus
 * 
 * Este módulo implementa a comunicação RS485 MODBUS RTU para:
 * - Câmeras LPR (Reconhecimento de Placas)
 * - Painel de Informações
 * - Controle de Cancelas Remotas
 */

#include "modbus_client.h"
#include "system_logger.h"
#include <modbus.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

// =============================================================================
// DEFINIÇÕES GLOBAIS
// =============================================================================

// Contextos MODBUS para cada dispositivo
static modbus_t *ctx_camera_entrada = NULL;
static modbus_t *ctx_camera_saida = NULL;  
static modbus_t *ctx_display = NULL;

// Estado de inicialização
static bool modbus_initialized = false;

// Configuração padrão RS485
#define MODBUS_BAUD_RATE 9600
#define MODBUS_DATA_BITS 8
#define MODBUS_PARITY 'N'  // None
#define MODBUS_STOP_BITS 1
#define MODBUS_TIMEOUT_SEC 1
#define MODBUS_TIMEOUT_USEC 0

// =============================================================================
// FUNÇÕES PRIVADAS
// =============================================================================

/**
 * @brief Configura parâmetros específicos do contexto MODBUS
 * @param ctx Contexto MODBUS
 * @return 0 se sucesso, -1 se erro
 */
static int configure_modbus_context(modbus_t *ctx) {
    if (!ctx) return -1;
    
    // Configurar timeout
    if (modbus_set_response_timeout(ctx, MODBUS_TIMEOUT_SEC, MODBUS_TIMEOUT_USEC) == -1) {
        LOG_ERROR("MODBUS", "Erro ao configurar timeout: %s", modbus_strerror(errno));
        return -1;
    }
    
    // Configurar timeout de byte
    if (modbus_set_byte_timeout(ctx, MODBUS_TIMEOUT_SEC, MODBUS_TIMEOUT_USEC) == -1) {
        LOG_ERROR("MODBUS", "Erro ao configurar byte timeout: %s", modbus_strerror(errno));
        return -1;
    }
    
    // Habilitar modo debug se compilado em DEBUG
    #ifdef DEBUG
    modbus_set_debug(ctx, TRUE);
    #endif
    
    return 0;
}

/**
 * @brief Conecta a um dispositivo MODBUS RTU
 * @param ctx Contexto MODBUS
 * @param device_name Nome do dispositivo para log
 * @return 0 se sucesso, -1 se erro
 */
static int connect_modbus_device(modbus_t *ctx, const char *device_name) {
    if (!ctx || !device_name) return -1;
    
    if (modbus_connect(ctx) == -1) {
        LOG_ERROR("MODBUS", "Falha ao conectar %s: %s", device_name, modbus_strerror(errno));
        return -1;
    }
    
    LOG_INFO("MODBUS", "%s conectado com sucesso", device_name);
    return 0;
}

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

/**
 * @brief Inicializa o cliente MODBUS
 * @param serial_port Porta serial (ex: "/dev/ttyUSB0")
 * @return 0 se sucesso, -1 se erro
 */
int modbus_init(const char *serial_port) {
    if (modbus_initialized) {
        LOG_WARN("MODBUS", "MODBUS já inicializado");
        return 0;
    }
    
    if (!serial_port) {
        LOG_ERROR("MODBUS", "Porta serial não especificada");
        return -1;
    }
    
    LOG_INFO("MODBUS", "Inicializando cliente MODBUS na porta %s", serial_port);
    
    // Criar contexto para câmera de entrada (endereço 0x11)
    ctx_camera_entrada = modbus_new_rtu(serial_port, MODBUS_BAUD_RATE, 
                                      MODBUS_PARITY, MODBUS_DATA_BITS, MODBUS_STOP_BITS);
    if (!ctx_camera_entrada) {
        LOG_ERROR("MODBUS", "Erro ao criar contexto câmera entrada: %s", modbus_strerror(errno));
        return -1;
    }
    
    // Configurar endereço slave para câmera de entrada
    if (modbus_set_slave(ctx_camera_entrada, MODBUS_ADDR_CAMERA_ENTRADA) == -1) {
        LOG_ERROR("MODBUS", "Erro ao configurar endereço câmera entrada: %s", modbus_strerror(errno));
        modbus_free(ctx_camera_entrada);
        ctx_camera_entrada = NULL;
        return -1;
    }
    
    // Criar contexto para câmera de saída (endereço 0x12)
    ctx_camera_saida = modbus_new_rtu(serial_port, MODBUS_BAUD_RATE, 
                                    MODBUS_PARITY, MODBUS_DATA_BITS, MODBUS_STOP_BITS);
    if (!ctx_camera_saida) {
        LOG_ERROR("MODBUS", "Erro ao criar contexto câmera saída: %s", modbus_strerror(errno));
        modbus_free(ctx_camera_entrada);
        ctx_camera_entrada = NULL;
        return -1;
    }
    
    // Configurar endereço slave para câmera de saída
    if (modbus_set_slave(ctx_camera_saida, MODBUS_ADDR_CAMERA_SAIDA) == -1) {
        LOG_ERROR("MODBUS", "Erro ao configurar endereço câmera saída: %s", modbus_strerror(errno));
        modbus_free(ctx_camera_entrada);
        modbus_free(ctx_camera_saida);
        ctx_camera_entrada = NULL;
        ctx_camera_saida = NULL;
        return -1;
    }
    
    // Criar contexto para display (endereço 0x20)
    ctx_display = modbus_new_rtu(serial_port, MODBUS_BAUD_RATE, 
                               MODBUS_PARITY, MODBUS_DATA_BITS, MODBUS_STOP_BITS);
    if (!ctx_display) {
        LOG_ERROR("MODBUS", "Erro ao criar contexto display: %s", modbus_strerror(errno));
        modbus_free(ctx_camera_entrada);
        modbus_free(ctx_camera_saida);
        ctx_camera_entrada = NULL;
        ctx_camera_saida = NULL;
        return -1;
    }
    
    // Configurar endereço slave para display
    if (modbus_set_slave(ctx_display, MODBUS_ADDR_DISPLAY) == -1) {
        LOG_ERROR("MODBUS", "Erro ao configurar endereço display: %s", modbus_strerror(errno));
        modbus_free(ctx_camera_entrada);
        modbus_free(ctx_camera_saida);
        modbus_free(ctx_display);
        ctx_camera_entrada = NULL;
        ctx_camera_saida = NULL;
        ctx_display = NULL;
        return -1;
    }
    
    // Configurar todos os contextos
    if (configure_modbus_context(ctx_camera_entrada) == -1 ||
        configure_modbus_context(ctx_camera_saida) == -1 ||
        configure_modbus_context(ctx_display) == -1) {
        modbus_cleanup();
        return -1;
    }
    
    // Conectar aos dispositivos
    if (connect_modbus_device(ctx_camera_entrada, "Câmera Entrada") == -1 ||
        connect_modbus_device(ctx_camera_saida, "Câmera Saída") == -1 ||
        connect_modbus_device(ctx_display, "Display") == -1) {
        modbus_cleanup();
        return -1;
    }
    
    modbus_initialized = true;
    LOG_INFO("MODBUS", "Cliente MODBUS inicializado com sucesso");
    return 0;
}

/**
 * @brief Finaliza o cliente MODBUS
 */
void modbus_cleanup(void) {
    if (!modbus_initialized) return;
    
    LOG_INFO("MODBUS", "Finalizando cliente MODBUS...");
    
    if (ctx_camera_entrada) {
        modbus_close(ctx_camera_entrada);
        modbus_free(ctx_camera_entrada);
        ctx_camera_entrada = NULL;
    }
    
    if (ctx_camera_saida) {
        modbus_close(ctx_camera_saida);
        modbus_free(ctx_camera_saida);
        ctx_camera_saida = NULL;
    }
    
    if (ctx_display) {
        modbus_close(ctx_display);
        modbus_free(ctx_display);
        ctx_display = NULL;
    }
    
    modbus_initialized = false;
    LOG_INFO("MODBUS", "Cliente MODBUS finalizado");
}

/**
 * @brief Lê placa de um veículo da câmera LPR
 * @param camera_type Tipo de câmera (ENTRADA ou SAIDA)
 * @param plate_buffer Buffer para armazenar a placa (min 16 bytes)
 * @param buffer_size Tamanho do buffer
 * @return 0 se sucesso, -1 se erro
 */
int modbus_read_license_plate(modbus_camera_type_t camera_type, char *plate_buffer, size_t buffer_size) {
    if (!modbus_initialized || !plate_buffer || buffer_size < 16) {
        LOG_ERROR("MODBUS", "Parâmetros inválidos para leitura de placa");
        return -1;
    }
    
    modbus_t *ctx = (camera_type == MODBUS_CAMERA_ENTRADA) ? ctx_camera_entrada : ctx_camera_saida;
    const char *camera_name = (camera_type == MODBUS_CAMERA_ENTRADA) ? "Entrada" : "Saída";
    
    if (!ctx) {
        LOG_ERROR("MODBUS", "Contexto da câmera %s não inicializado", camera_name);
        return -1;
    }
    
    // Ler dados da placa (registers 0-7 = 16 bytes)
    uint16_t plate_registers[8];
    int ret = modbus_read_holding_registers(ctx, MODBUS_REG_LICENSE_PLATE, 8, plate_registers);
    
    if (ret == -1) {
        LOG_ERROR("MODBUS", "Erro ao ler placa da câmera %s: %s", camera_name, modbus_strerror(errno));
        return -1;
    }
    
    // Converter registradores para string
    memset(plate_buffer, 0, buffer_size);
    for (int i = 0; i < 8 && (i * 2) < (buffer_size - 1); i++) {
        plate_buffer[i * 2] = (char)(plate_registers[i] >> 8);       // Byte alto
        plate_buffer[i * 2 + 1] = (char)(plate_registers[i] & 0xFF); // Byte baixo
    }
    
    // Remover caracteres de controle e garantir terminação
    for (size_t i = 0; i < buffer_size - 1; i++) {
        if (plate_buffer[i] < 32 || plate_buffer[i] > 126) {
            plate_buffer[i] = '\0';
            break;
        }
    }
    plate_buffer[buffer_size - 1] = '\0';
    
    LOG_INFO("MODBUS", "Placa lida da câmera %s: '%s'", camera_name, plate_buffer);
    return 0;
}

/**
 * @brief Verifica se há uma nova leitura disponível na câmera
 * @param camera_type Tipo de câmera
 * @return true se há nova leitura, false caso contrário
 */
bool modbus_has_new_reading(modbus_camera_type_t camera_type) {
    if (!modbus_initialized) return false;
    
    modbus_t *ctx = (camera_type == MODBUS_CAMERA_ENTRADA) ? ctx_camera_entrada : ctx_camera_saida;
    const char *camera_name = (camera_type == MODBUS_CAMERA_ENTRADA) ? "Entrada" : "Saída";
    
    if (!ctx) {
        LOG_ERROR("MODBUS", "Contexto da câmera %s não inicializado", camera_name);
        return false;
    }
    
    // Ler status da câmera
    uint16_t status;
    int ret = modbus_read_holding_registers(ctx, MODBUS_REG_CAMERA_STATUS, 1, &status);
    
    if (ret == -1) {
        LOG_DEBUG("MODBUS", "Erro ao ler status da câmera %s: %s", camera_name, modbus_strerror(errno));
        return false;
    }
    
    // Bit 0 indica nova leitura disponível
    return (status & 0x01) != 0;
}

/**
 * @brief Reconhece a leitura de uma placa (limpa flag de nova leitura)
 * @param camera_type Tipo de câmera
 * @return 0 se sucesso, -1 se erro
 */
int modbus_acknowledge_reading(modbus_camera_type_t camera_type) {
    if (!modbus_initialized) return -1;
    
    modbus_t *ctx = (camera_type == MODBUS_CAMERA_ENTRADA) ? ctx_camera_entrada : ctx_camera_saida;
    const char *camera_name = (camera_type == MODBUS_CAMERA_ENTRADA) ? "Entrada" : "Saída";
    
    if (!ctx) {
        LOG_ERROR("MODBUS", "Contexto da câmera %s não inicializado", camera_name);
        return -1;
    }
    
    // Escrever 1 no registro de acknowledge para limpar flag
    uint16_t ack_value = 1;
    int ret = modbus_write_single_register(ctx, MODBUS_REG_CAMERA_ACK, ack_value);
    
    if (ret == -1) {
        LOG_ERROR("MODBUS", "Erro ao reconhecer leitura da câmera %s: %s", camera_name, modbus_strerror(errno));
        return -1;
    }
    
    LOG_DEBUG("MODBUS", "Leitura da câmera %s reconhecida", camera_name);
    return 0;
}

/**
 * @brief Atualiza informações no display
 * @param info Estrutura com informações para exibir
 * @return 0 se sucesso, -1 se erro
 */
int modbus_update_display(const modbus_display_info_t *info) {
    if (!modbus_initialized || !info) {
        LOG_ERROR("MODBUS", "Parâmetros inválidos para atualização do display");
        return -1;
    }
    
    if (!ctx_display) {
        LOG_ERROR("MODBUS", "Contexto do display não inicializado");
        return -1;
    }
    
    // Preparar dados para envio
    uint16_t display_data[MODBUS_DISPLAY_REGS];
    
    // Vagas disponíveis por andar (registradores 0-2)
    display_data[0] = (uint16_t)info->vagas_terreo;
    display_data[1] = (uint16_t)info->vagas_andar1;
    display_data[2] = (uint16_t)info->vagas_andar2;
    
    // Total de vagas (registrador 3)
    display_data[3] = (uint16_t)info->total_vagas;
    
    // Status do sistema (registrador 4)
    display_data[4] = (uint16_t)info->sistema_status;
    
    // Hora atual (registradores 5-6)
    display_data[5] = (uint16_t)(info->hora_atual >> 16); // Parte alta
    display_data[6] = (uint16_t)(info->hora_atual & 0xFFFF); // Parte baixa
    
    // Mensagem de texto (registradores 7-14, 16 caracteres)
    for (int i = 0; i < 8; i++) {
        int idx = i * 2;
        if (idx < 15 && info->mensagem[idx] != '\0') {
            display_data[7 + i] = ((uint16_t)info->mensagem[idx] << 8) | 
                                ((idx + 1 < 16) ? (uint16_t)info->mensagem[idx + 1] : 0);
        } else {
            display_data[7 + i] = 0;
        }
    }
    
    // Escrever dados no display
    int ret = modbus_write_multiple_registers(ctx_display, MODBUS_REG_DISPLAY_VAGAS_TERREO, 
                                            MODBUS_DISPLAY_REGS, display_data);
    
    if (ret == -1) {
        LOG_ERROR("MODBUS", "Erro ao atualizar display: %s", modbus_strerror(errno));
        return -1;
    }
    
    LOG_INFO("MODBUS", "Display atualizado: T=%d A1=%d A2=%d Total=%d", 
             info->vagas_terreo, info->vagas_andar1, info->vagas_andar2, info->total_vagas);
    return 0;
}

/**
 * @brief Testa comunicação com todos os dispositivos MODBUS
 * @return 0 se todos OK, -1 se algum erro
 */
int modbus_test_communication(void) {
    if (!modbus_initialized) {
        LOG_ERROR("MODBUS", "MODBUS não inicializado para teste");
        return -1;
    }
    
    LOG_INFO("MODBUS", "Testando comunicação com dispositivos MODBUS...");
    
    int errors = 0;
    
    // Testar câmera de entrada
    uint16_t test_reg;
    if (modbus_read_holding_registers(ctx_camera_entrada, MODBUS_REG_CAMERA_STATUS, 1, &test_reg) == -1) {
        LOG_ERROR("MODBUS", "Câmera de entrada não responde: %s", modbus_strerror(errno));
        errors++;
    } else {
        LOG_INFO("MODBUS", "✓ Câmera de entrada OK (status: 0x%04X)", test_reg);
    }
    
    // Testar câmera de saída
    if (modbus_read_holding_registers(ctx_camera_saida, MODBUS_REG_CAMERA_STATUS, 1, &test_reg) == -1) {
        LOG_ERROR("MODBUS", "Câmera de saída não responde: %s", modbus_strerror(errno));
        errors++;
    } else {
        LOG_INFO("MODBUS", "✓ Câmera de saída OK (status: 0x%04X)", test_reg);
    }
    
    // Testar display
    if (modbus_read_holding_registers(ctx_display, MODBUS_REG_DISPLAY_VAGAS_TERREO, 1, &test_reg) == -1) {
        LOG_ERROR("MODBUS", "Display não responde: %s", modbus_strerror(errno));
        errors++;
    } else {
        LOG_INFO("MODBUS", "✓ Display OK (vagas térreo: %d)", test_reg);
    }
    
    if (errors == 0) {
        LOG_INFO("MODBUS", "Todos os dispositivos MODBUS estão funcionando");
        return 0;
    } else {
        LOG_ERROR("MODBUS", "%d dispositivo(s) com problema", errors);
        return -1;
    }
}