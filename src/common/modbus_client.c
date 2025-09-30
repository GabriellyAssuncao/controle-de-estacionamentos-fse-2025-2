/**
 * @file modbus_client.c
 * @brief Implementação  cliente MODBUS RTU
 */

#include "modbus_client.h"
#include "system_logger.h"
#include <modbus/modbus.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
 
// VARIÁVEIS GLOBAIS PRIVADAS 

static modbus_t *ctx = NULL;
static bool initialized = false;
static modbus_stats_t stats = {0};
static int current_timeout_ms = MODBUS_TIMEOUT_USEC / 1000;
static int current_retries = MODBUS_MAX_RETRIES;
static bool debug_enabled = false;
 
// FUNÇÕES AUXILIARES PRIVADAS  

/**
 * @brief Executa operação MODBUS com retentativas
 */
static int modbus_execute_with_retry(int (*operation)(void), const char* op_name) {
    int attempt = 0;
    int result;
    
    while (attempt < current_retries) {
        result = operation();
        
        if (result != -1) {
            stats.responses_received++;
            return result;
        }
        
        // Erro - verificar tipo
        if (errno == ETIMEDOUT || errno == ECONNRESET) {
            stats.timeouts++;
        } else if (errno == EMBBADCRC) {
            stats.crc_errors++;
        } else {
            stats.errors++;
        }
        
        attempt++;
        
        if (attempt < current_retries) {
            int delay = MODBUS_RETRY_DELAY_MS * (1 << (attempt - 1)); // Backoff exponencial
            LOG_WARN("MODBUS", "%s falhou (tentativa %d/%d): %s. Aguardando %dms...", 
                     op_name, attempt, current_retries, modbus_strerror(errno), delay);
            usleep(delay * 1000);
        }
    }
    
    LOG_ERROR("MODBUS", "%s falhou após %d tentativas", op_name, current_retries);
    stats.errors++;
    return -1;
}

/**
 * @brief Valida e limpa string de placa
 */
static void sanitize_plate_string(char* plate) {
    // Remove caracteres não imprimíveis
    for (int i = 0; i < 8; i++) {
        if (plate[i] < 32 || plate[i] > 126) {
            plate[i] = '\0';
            break;
        }
    }
    plate[8] = '\0';
    
    // Remove espaços no final
    int len = strlen(plate);
    while (len > 0 && plate[len-1] == ' ') {
        plate[--len] = '\0';
    }
}
 
// FUNÇÕES PÚBLICAS - INICIALIZAÇÃO 

int modbus_init(const char* device, int baudrate) {
    if (initialized) {
        LOG_WARN("MODBUS", "Cliente já inicializado");
        return 0;
    }
    
    if (!device) {
        device = MODBUS_DEFAULT_DEVICE;
    }
    
    if (baudrate <= 0) {
        baudrate = MODBUS_DEFAULT_BAUDRATE;
    }
    
    LOG_INFO("MODBUS", "Inicializando cliente MODBUS RTU");
    LOG_INFO("MODBUS", "  Dispositivo: %s", device);
    LOG_INFO("MODBUS", "  Baudrate: %d bps", baudrate);
    LOG_INFO("MODBUS", "  Paridade: N, Data: 8, Stop: 1");
    
    // Criar contexto RTU
    ctx = modbus_new_rtu(device, baudrate, 'N', 8, 1);
    if (ctx == NULL) {
        LOG_ERROR("MODBUS", "Erro ao criar contexto RTU: %s", modbus_strerror(errno));
        return -1;
    }
    
    // Configurar timeouts
    struct timeval response_timeout;
    response_timeout.tv_sec = MODBUS_TIMEOUT_SEC;
    response_timeout.tv_usec = MODBUS_TIMEOUT_USEC;
    modbus_set_response_timeout(ctx, response_timeout.tv_sec, response_timeout.tv_usec);
    
    struct timeval byte_timeout;
    byte_timeout.tv_sec = 0;
    byte_timeout.tv_usec = 100000; // 100ms entre bytes
    modbus_set_byte_timeout(ctx, byte_timeout.tv_sec, byte_timeout.tv_usec);
    
    // Habilitar debug se necessário
    if (debug_enabled) {
        modbus_set_debug(ctx, TRUE);
    }
    
    // Conectar
    if (modbus_connect(ctx) == -1) {
        LOG_ERROR("MODBUS", "Erro ao conectar ao barramento: %s", modbus_strerror(errno));
        modbus_free(ctx);
        ctx = NULL;
        return -1;
    }
    
    // Limpar estatísticas
    memset(&stats, 0, sizeof(stats));
    
    initialized = true;
    LOG_INFO("MODBUS", "Cliente MODBUS inicializado com sucesso");
    
    return 0;
}

void modbus_cleanup(void) {
    if (!initialized) return;
    
    LOG_INFO("MODBUS", "Finalizando cliente MODBUS");
    
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = NULL;
    }
    
    initialized = false;
}

bool modbus_is_initialized(void) {
    return initialized;
}
 
// FUNÇÕES DE CÂMERA LPR 

int modbus_camera_trigger(camera_type_t camera) {
    if (!initialized || !ctx) {
        LOG_ERROR("MODBUS", "Cliente não inicializado");
        return -1;
    }
    
    LOG_DEBUG("MODBUS", "Disparando câmera 0x%02X", camera);
    
    // Selecionar slave
    modbus_set_slave(ctx, camera);
    
    // Escrever 1 no registrador TRIGGER
    uint16_t trigger_value = 1;
    
    stats.requests_sent++;
    if (modbus_write_register(ctx, LPR_REG_TRIGGER, trigger_value) == -1) {
        LOG_ERROR("MODBUS", "Erro ao disparar câmera 0x%02X: %s", 
                  camera, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    LOG_INFO("MODBUS", "Câmera 0x%02X disparada com sucesso", camera);
    return 0;
}

int modbus_camera_get_status(camera_type_t camera) {
    if (!initialized || !ctx) {
        LOG_ERROR("MODBUS", "Cliente não inicializado");
        return -1;
    }
    
    modbus_set_slave(ctx, camera);
    
    uint16_t status;
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, LPR_REG_STATUS, 1, &status) != 1) {
        LOG_DEBUG("MODBUS", "Erro ao ler status da câmera 0x%02X: %s", 
                  camera, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    return (int)status;
}

int modbus_camera_read_plate(camera_type_t camera, plate_reading_t* result, int timeout_ms) {
    if (!initialized || !ctx || !result) {
        LOG_ERROR("MODBUS", "Parâmetros inválidos");
        return -1;
    }
    
    memset(result, 0, sizeof(plate_reading_t));
    result->timestamp = time(NULL);
    
    if (timeout_ms <= 0) {
        timeout_ms = 2000;  
    }
    
    const char* camera_name = (camera == CAMERA_ENTRADA) ? "ENTRADA" : "SAÍDA";
    LOG_DEBUG("MODBUS", "Aguardando leitura da câmera %s (timeout: %dms)", 
              camera_name, timeout_ms);
    
    modbus_set_slave(ctx, camera);
    
    // Polling de status
    int elapsed = 0;
    const int poll_interval = 100; // 100ms
    int status = -1;
    
    while (elapsed < timeout_ms) {
        status = modbus_camera_get_status(camera);
        
        if (status == LPR_STATUS_OK) {
            LOG_DEBUG("MODBUS", "Câmera %s: captura OK", camera_name);
            break;
        } else if (status == LPR_STATUS_ERROR) {
            LOG_ERROR("MODBUS", "Câmera %s retornou erro", camera_name);
            return -1;
        } else if (status == LPR_STATUS_PROCESSING) {
            LOG_DEBUG("MODBUS", "Câmera %s: processando...", camera_name);
        }
        
        usleep(poll_interval * 1000);
        elapsed += poll_interval;
    }
    
    if (status != LPR_STATUS_OK) {
        LOG_ERROR("MODBUS", "Timeout aguardando câmera %s", camera_name);
        stats.timeouts++;
        return -1;
    }
    
    // Ler placa (4 registradores = 8 bytes)
    uint16_t plate_regs[4];
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, LPR_REG_PLATE, 4, plate_regs) != 4) {
        LOG_ERROR("MODBUS", "Erro ao ler placa da câmera %s: %s", 
                  camera_name, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    
    // Converter registradores para string
    for (int i = 0; i < 4; i++) {
        result->plate[i*2] = (plate_regs[i] >> 8) & 0xFF;     // High byte
        result->plate[i*2 + 1] = plate_regs[i] & 0xFF;        // Low byte
    }
    result->plate[8] = '\0';
    
    // Limpar string
    sanitize_plate_string(result->plate);
    
    // Ler confiança
    uint16_t confidence_reg;
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, LPR_REG_CONFIDENCE, 1, &confidence_reg) != 1) {
        LOG_WARN("MODBUS", "Erro ao ler confiança da câmera %s: %s", 
                 camera_name, modbus_strerror(errno));
        result->confidence = 0;
        stats.errors++;
    } else {
        result->confidence = confidence_reg;
        stats.responses_received++;
    }
    
    result->success = (result->confidence >= MIN_PLATE_CONFIDENCE && 
                       strlen(result->plate) >= 7);
    
    LOG_INFO("MODBUS", "Placa lida da câmera %s: '%s' (confiança: %d%%) %s", 
             camera_name, result->plate, result->confidence,
             result->success ? "[OK]" : "[BAIXA CONFIANÇA]");
    
    return 0;
}

int modbus_camera_capture_and_read(camera_type_t camera, plate_reading_t* result) {
    if (!initialized || !result) {
        return -1;
    }
    
    // Disparar captura
    if (modbus_camera_trigger(camera) != 0) {
        return -1;
    }
    
    // Aguardar e ler resultado
    return modbus_camera_read_plate(camera, result, 2000);
}

int modbus_camera_reset(camera_type_t camera) {
    if (!initialized || !ctx) {
        return -1;
    }
    
    modbus_set_slave(ctx, camera);
    
    // Escrever 0 no registrador TRIGGER
    uint16_t reset_value = 0;
    
    stats.requests_sent++;
    if (modbus_write_register(ctx, LPR_REG_TRIGGER, reset_value) == -1) {
        LOG_ERROR("MODBUS", "Erro ao resetar câmera 0x%02X: %s", 
                  camera, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    LOG_DEBUG("MODBUS", "Câmera 0x%02X resetada", camera);
    return 0;
}
 
// FUNÇÕES DE DISPLAY 

int modbus_display_update(const display_info_t* info) {
    if (!initialized || !ctx || !info) {
        LOG_ERROR("MODBUS", "Parâmetros inválidos");
        return -1;
    }
    
    LOG_DEBUG("MODBUS", "Atualizando display completo");
    
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    // Preparar dados (13 registradores)
    uint16_t data[13];
    
    // Térreo
    data[0] = info->terreo_pne;
    data[1] = info->terreo_idoso;
    data[2] = info->terreo_comum;
    
    // Andar 1
    data[3] = info->andar1_pne;
    data[4] = info->andar1_idoso;
    data[5] = info->andar1_comum;
    
    // Andar 2
    data[6] = info->andar2_pne;
    data[7] = info->andar2_idoso;
    data[8] = info->andar2_comum;
    
    // Totais
    data[9] = info->total_pne;
    data[10] = info->total_idoso;
    data[11] = info->total_comum;
    
    // Flags
    data[12] = 0;
    if (info->lotado_geral) data[12] |= DISPLAY_FLAG_LOTADO_GERAL;
    if (info->bloqueado_andar1) data[12] |= DISPLAY_FLAG_BLOQ_ANDAR1;
    if (info->bloqueado_andar2) data[12] |= DISPLAY_FLAG_BLOQ_ANDAR2;
    
    // Escrever todos os registradores
    stats.requests_sent++;
    
    if (modbus_write_registers(ctx, DISPLAY_REG_TERREO_PNE, 13, data) == -1) {
        LOG_ERROR("MODBUS", "Erro ao atualizar display: %s", modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    
    LOG_INFO("MODBUS", "Display atualizado: T=%d/%d/%d A1=%d/%d/%d A2=%d/%d/%d Total=%d/%d/%d", 
             info->terreo_pne, info->terreo_idoso, info->terreo_comum,
             info->andar1_pne, info->andar1_idoso, info->andar1_comum,
             info->andar2_pne, info->andar2_idoso, info->andar2_comum,
             info->total_pne, info->total_idoso, info->total_comum);
    
    return 0;
}

int modbus_display_update_floor(int floor, uint8_t pne, uint8_t idoso, uint8_t comum) {
    if (!initialized || !ctx || floor < 0 || floor > 2) {
        return -1;
    }
    
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    uint16_t data[3] = {pne, idoso, comum};
    int start_reg = DISPLAY_REG_TERREO_PNE + (floor * 3);
    
    stats.requests_sent++;
    
    if (modbus_write_registers(ctx, start_reg, 3, data) == -1) {
        LOG_ERROR("MODBUS", "Erro ao atualizar andar %d no display: %s", 
                  floor, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    LOG_DEBUG("MODBUS", "Display andar %d atualizado: %d/%d/%d", floor, pne, idoso, comum);
    return 0;
}

int modbus_display_update_flags(bool lotado, bool bloq_andar1, bool bloq_andar2) {
    if (!initialized || !ctx) {
        return -1;
    }
    
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    uint16_t flags = 0;
    if (lotado) flags |= DISPLAY_FLAG_LOTADO_GERAL;
    if (bloq_andar1) flags |= DISPLAY_FLAG_BLOQ_ANDAR1;
    if (bloq_andar2) flags |= DISPLAY_FLAG_BLOQ_ANDAR2;
    
    stats.requests_sent++;
    
    if (modbus_write_register(ctx, DISPLAY_REG_FLAGS, flags) == -1) {
        LOG_ERROR("MODBUS", "Erro ao atualizar flags do display: %s", 
                  modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    LOG_DEBUG("MODBUS", "Flags do display atualizadas: lotado=%d bloq1=%d bloq2=%d", 
              lotado, bloq_andar1, bloq_andar2);
    return 0;
}

int modbus_display_read(display_info_t* info) {
    if (!initialized || !ctx || !info) {
        return -1;
    }
    
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    uint16_t data[13];
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, DISPLAY_REG_TERREO_PNE, 13, data) != 13) {
        LOG_ERROR("MODBUS", "Erro ao ler display: %s", modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    
    // Parsear dados
    info->terreo_pne = data[0];
    info->terreo_idoso = data[1];
    info->terreo_comum = data[2];
    
    info->andar1_pne = data[3];
    info->andar1_idoso = data[4];
    info->andar1_comum = data[5];
    
    info->andar2_pne = data[6];
    info->andar2_idoso = data[7];
    info->andar2_comum = data[8];
    
    info->total_pne = data[9];
    info->total_idoso = data[10];
    info->total_comum = data[11];
    
    // Flags
    uint16_t flags = data[12];
    info->lotado_geral = (flags & DISPLAY_FLAG_LOTADO_GERAL) != 0;
    info->bloqueado_andar1 = (flags & DISPLAY_FLAG_BLOQ_ANDAR1) != 0;
    info->bloqueado_andar2 = (flags & DISPLAY_FLAG_BLOQ_ANDAR2) != 0;
    
    return 0;
}
 
// FUNÇÕES DE DIAGNÓSTICO 

int modbus_test_device(uint8_t address) {
    if (!initialized || !ctx) {
        return -1;
    }
    
    LOG_DEBUG("MODBUS", "Testando dispositivo 0x%02X", address);
    
    modbus_set_slave(ctx, address);
    
    // Tentar ler 1 registrador (status ou primeiro registrador)
    uint16_t test_reg;
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, 0, 1, &test_reg) != 1) {
        LOG_WARN("MODBUS", "Dispositivo 0x%02X não responde: %s", 
                 address, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    LOG_INFO("MODBUS", "Dispositivo 0x%02X: OK (reg[0]=0x%04X)", address, test_reg);
    return 0;
}

int modbus_test_all_devices(void) {
    if (!initialized) {
        LOG_ERROR("MODBUS", "Cliente não inicializado");
        return 0;
    }
    
    LOG_INFO("MODBUS", "=== TESTE DE DISPOSITIVOS MODBUS ===");
    
    int devices_ok = 0;
    
    // Testar câmera de entrada
    printf("Câmera Entrada (0x%02X): ", MODBUS_ADDR_CAMERA_ENTRADA);
    if (modbus_test_device(MODBUS_ADDR_CAMERA_ENTRADA) == 0) {
        printf("✓ OK\n");
        devices_ok++;
    } else {
        printf("✗ FALHA\n");
    }
    
    // Testar câmera de saída
    printf("Câmera Saída (0x%02X):    ", MODBUS_ADDR_CAMERA_SAIDA);
    if (modbus_test_device(MODBUS_ADDR_CAMERA_SAIDA) == 0) {
        printf("✓ OK\n");
        devices_ok++;
    } else {
        printf("✗ FALHA\n");
    }
    
    // Testar display
    printf("Display (0x%02X):         ", MODBUS_ADDR_DISPLAY);
    if (modbus_test_device(MODBUS_ADDR_DISPLAY) == 0) {
        printf("✓ OK\n");
        devices_ok++;
    } else {
        printf("✗ FALHA\n");
    }
    
    printf("=====================================\n");
    LOG_INFO("MODBUS", "%d de 3 dispositivos respondendo", devices_ok);
    
    return devices_ok;
}

void modbus_get_stats(modbus_stats_t* stats_out) {
    if (stats_out) {
        memcpy(stats_out, &stats, sizeof(modbus_stats_t));
    }
}

void modbus_reset_stats(void) {
    memset(&stats, 0, sizeof(modbus_stats_t));
    LOG_INFO("MODBUS", "Estatísticas resetadas");
}

void modbus_print_diagnostics(void) {
    printf("\n=== DIAGNÓSTICO MODBUS ===\n");
    printf("Status: %s\n", initialized ? "INICIALIZADO" : "NÃO INICIALIZADO");
    
    if (initialized) {
        printf("\nEstatísticas:\n");
        printf("  Requisições enviadas:  %u\n", stats.requests_sent);
        printf("  Respostas recebidas:   %u\n", stats.responses_received);
        printf("  Erros:                 %u\n", stats.errors);
        printf("  Timeouts:              %u\n", stats.timeouts);
        printf("  Erros CRC:             %u\n", stats.crc_errors);
        
        if (stats.requests_sent > 0) {
            float success_rate = (float)stats.responses_received / stats.requests_sent * 100.0f;
            printf("  Taxa de sucesso:       %.1f%%\n", success_rate);
        }
        
        printf("\nConfiguração:\n");
        printf("  Timeout:               %d ms\n", current_timeout_ms);
        printf("  Retentativas:          %d\n", current_retries);
        printf("  Debug:                 %s\n", debug_enabled ? "ATIVO" : "INATIVO");
    }
    
    printf("==========================\n\n");
}
 
// FUNÇÕES DE CONFIGURAÇÃO 
void modbus_set_timeout(int timeout_ms) {
    current_timeout_ms = timeout_ms;
    
    if (ctx) {
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        modbus_set_response_timeout(ctx, timeout.tv_sec, timeout.tv_usec);
    }
    
    LOG_INFO("MODBUS", "Timeout configurado para %d ms", timeout_ms);
}

void modbus_set_retries(int retries) {
    if (retries >= 0 && retries <= 10) {
        current_retries = retries;
        LOG_INFO("MODBUS", "Retentativas configuradas para %d", retries);
    } else {
        LOG_WARN("MODBUS", "Número de retentativas inválido: %d (deve ser 0-10)", retries);
    }
}

void modbus_set_debug(bool enable) {
    debug_enabled = enable;
    
    if (ctx) {
        modbus_set_debug(ctx, enable ? TRUE : FALSE);
    }
    
    LOG_INFO("MODBUS", "Debug MODBUS %s", enable ? "HABILITADO" : "DESABILITADO");
}