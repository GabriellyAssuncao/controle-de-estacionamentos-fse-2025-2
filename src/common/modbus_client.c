/**
 * @file modbus_client.c
 * @brief Cliente MODBUS RTU completo para comunicação com dispositivos
 */

#include "modbus_client.h"
#include "system_logger.h"
#include <modbus/modbus.h>
#include <string.h>
#include <unistd.h>

// =============================================================================
// VARIÁVEIS GLOBAIS
// =============================================================================

static modbus_t *ctx = NULL;
static bool initialized = false;
static modbus_stats_t stats = {0};
static pthread_mutex_t modbus_mutex = PTHREAD_MUTEX_INITIALIZER;

// Configurações
static int current_timeout_ms = MODBUS_TIMEOUT_MS;
static int current_retries = MODBUS_MAX_RETRIES;
static bool debug_enabled = false;

// =============================================================================
// FUNÇÕES AUXILIARES
// =============================================================================

/**
 * @brief Calcula CRC16 MODBUS
 */
static uint16_t modbus_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief Adiciona matrícula ao final da mensagem MODBUS
 */
static int add_matricula_to_message(uint8_t* buffer, int length) {
    const char* matricula = MODBUS_MATRICULA;
    size_t mat_len = strlen(matricula);
    
    if (mat_len < 4) {
        LOG_ERROR("MODBUS", "Matrícula inválida: deve ter pelo menos 4 dígitos");
        return length;
    }
    
    // Pega os últimos 4 dígitos
    const char* last_4 = matricula + (mat_len - 4);
    
    // Converte para dois registros de 16 bits
    uint16_t mat_part1 = ((last_4[0] - '0') << 8) | (last_4[1] - '0');
    uint16_t mat_part2 = ((last_4[2] - '0') << 8) | (last_4[3] - '0');
    
    buffer[length++] = (mat_part1 >> 8) & 0xFF;
    buffer[length++] = mat_part1 & 0xFF;
    buffer[length++] = (mat_part2 >> 8) & 0xFF;
    buffer[length++] = mat_part2 & 0xFF;
    
    if (debug_enabled) {
        LOG_DEBUG("MODBUS", "Matrícula adicionada: %c%c%c%c", 
                  last_4[0], last_4[1], last_4[2], last_4[3]);
    }
    
    return length;
}

// =============================================================================
// FUNÇÕES PÚBLICAS - INICIALIZAÇÃO
// =============================================================================

int modbus_init(const char* device, int baudrate) {
    if (initialized) {
        LOG_WARN("MODBUS", "Cliente já inicializado");
        return 0;
    }
    
    LOG_INFO("MODBUS", "Inicializando cliente MODBUS RTU...");
    LOG_INFO("MODBUS", "  Dispositivo: %s", device);
    LOG_INFO("MODBUS", "  Baudrate: %d", baudrate);
    
    // Criar contexto MODBUS RTU
    ctx = modbus_new_rtu(device, baudrate, 'N', 8, 1);
    if (ctx == NULL) {
        LOG_ERROR("MODBUS", "Erro ao criar contexto: %s", modbus_strerror(errno));
        return -1;
    }
    
    // Configurar timeout
    struct timeval timeout;
    timeout.tv_sec = current_timeout_ms / 1000;
    timeout.tv_usec = (current_timeout_ms % 1000) * 1000;
    modbus_set_response_timeout(ctx, timeout.tv_sec, timeout.tv_usec);
    
    // Configurar modo debug se necessário
    if (debug_enabled) {
        modbus_set_debug(ctx, TRUE);
    }
    
    // Conectar
    if (modbus_connect(ctx) == -1) {
        LOG_ERROR("MODBUS", "Erro ao conectar: %s", modbus_strerror(errno));
        modbus_free(ctx);
        ctx = NULL;
        return -1;
    }
    
    // Zerar estatísticas
    memset(&stats, 0, sizeof(stats));
    
    initialized = true;
    LOG_INFO("MODBUS", "Cliente MODBUS inicializado com sucesso");
    
    return 0;
}

void modbus_cleanup(void) {
    if (!initialized) return;
    
    LOG_INFO("MODBUS", "Finalizando cliente MODBUS...");
    
    pthread_mutex_lock(&modbus_mutex);
    
    if (ctx) {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = NULL;
    }
    
    initialized = false;
    pthread_mutex_unlock(&modbus_mutex);
    
    LOG_INFO("MODBUS", "Cliente MODBUS finalizado");
}

bool modbus_is_initialized(void) {
    return initialized;
}

// =============================================================================
// FUNÇÕES PÚBLICAS - CÂMERA LPR
// =============================================================================

int modbus_camera_trigger(camera_type_t camera) {
    if (!initialized || !ctx) {
        LOG_ERROR("MODBUS", "Cliente não inicializado");
        return -1;
    }
    
    const char* camera_name = (camera == CAMERA_ENTRADA) ? "ENTRADA" : "SAÍDA";
    LOG_DEBUG("MODBUS", "Disparando câmera %s (0x%02X)", camera_name, camera);
    
    pthread_mutex_lock(&modbus_mutex);
    
    modbus_set_slave(ctx, camera);
    
    // Preparar mensagem Write Single Register (0x06)
    uint8_t req[256];
    int req_length = 0;
    
    req[req_length++] = camera;              // Endereço slave
    req[req_length++] = 0x06;                // Function code
    req[req_length++] = (LPR_REG_TRIGGER >> 8) & 0xFF;  // Registro HIGH
    req[req_length++] = LPR_REG_TRIGGER & 0xFF;         // Registro LOW
    req[req_length++] = 0x00;                // Valor HIGH (1)
    req[req_length++] = 0x01;                // Valor LOW
    
    // Adicionar matrícula antes do CRC
    req_length = add_matricula_to_message(req, req_length);
    
    // Calcular e adicionar CRC
    uint16_t crc = modbus_crc16(req, req_length);
    req[req_length++] = crc & 0xFF;
    req[req_length++] = (crc >> 8) & 0xFF;
    
    stats.requests_sent++;
    
    // Enviar requisição
    int rc = modbus_send_raw_request(ctx, req, req_length);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao disparar câmera %s: %s", 
                  camera_name, modbus_strerror(errno));
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    // Receber confirmação
    uint8_t rsp[MODBUS_RTU_MAX_ADU_LENGTH];
    rc = modbus_receive_confirmation(ctx, rsp);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao receber confirmação da câmera %s: %s",
                  camera_name, modbus_strerror(errno));
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    stats.responses_received++;
    pthread_mutex_unlock(&modbus_mutex);
    
    LOG_INFO("MODBUS", "Câmera %s disparada com sucesso", camera_name);
    return 0;
}

int modbus_camera_read_plate(camera_type_t camera, plate_reading_t* result, int timeout_ms) {
    if (!initialized || !ctx || !result) {
        LOG_ERROR("MODBUS", "Parâmetros inválidos");
        return -1;
    }
    
    memset(result, 0, sizeof(plate_reading_t));
    result->timestamp = time(NULL);
    
    if (timeout_ms <= 0) {
        timeout_ms = 2000; // Default 2 segundos
    }
    
    const char* camera_name = (camera == CAMERA_ENTRADA) ? "ENTRADA" : "SAÍDA";
    LOG_DEBUG("MODBUS", "Lendo placa da câmera %s (timeout: %dms)", 
              camera_name, timeout_ms);
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, camera);
    
    // Polling do status até OK ou ERROR
    int elapsed = 0;
    const int poll_interval = 100; // 100ms
    int status = -1;
    
    while (elapsed < timeout_ms) {
        uint16_t status_reg;
        stats.requests_sent++;
        
        if (modbus_read_registers(ctx, LPR_REG_STATUS, 1, &status_reg) != 1) {
            LOG_DEBUG("MODBUS", "Erro ao ler status da câmera %s", camera_name);
            stats.errors++;
        } else {
            stats.responses_received++;
            status = (int)status_reg;
            
            if (status == LPR_STATUS_OK) {
                LOG_DEBUG("MODBUS", "Câmera %s: captura OK", camera_name);
                break;
            } else if (status == LPR_STATUS_ERROR) {
                LOG_ERROR("MODBUS", "Câmera %s retornou erro", camera_name);
                pthread_mutex_unlock(&modbus_mutex);
                return -1;
            }
            // Status PROCESSING ou READY - continua polling
        }
        
        pthread_mutex_unlock(&modbus_mutex);
        usleep(poll_interval * 1000);
        pthread_mutex_lock(&modbus_mutex);
        elapsed += poll_interval;
    }
    
    if (status != LPR_STATUS_OK) {
        LOG_ERROR("MODBUS", "Timeout aguardando câmera %s", camera_name);
        stats.timeouts++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    // Ler placa (4 registros = 8 bytes)
    uint16_t plate_regs[4];
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, LPR_REG_PLATE, 4, plate_regs) != 4) {
        LOG_ERROR("MODBUS", "Erro ao ler placa da câmera %s: %s", 
                  camera_name, modbus_strerror(errno));
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    stats.responses_received++;
    
    // Converter registros para string
    for (int i = 0; i < 4; i++) {
        result->plate[i*2] = (plate_regs[i] >> 8) & 0xFF;
        result->plate[i*2 + 1] = plate_regs[i] & 0xFF;
    }
    result->plate[8] = '\0';
    
    // Limpar caracteres inválidos
    for (int i = 0; i < 8; i++) {
        if (result->plate[i] < 32 || result->plate[i] > 126) {
            result->plate[i] = '\0';
            break;
        }
    }
    
    // Remover espaços no final
    int len = strlen(result->plate);
    while (len > 0 && result->plate[len-1] == ' ') {
        result->plate[--len] = '\0';
    }
    
    // Ler confiança
    uint16_t confidence_reg;
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, LPR_REG_CONFIDENCE, 1, &confidence_reg) != 1) {
        LOG_WARN("MODBUS", "Erro ao ler confiança da câmera %s", camera_name);
        result->confidence = 0;
        stats.errors++;
    } else {
        result->confidence = confidence_reg;
        stats.responses_received++;
    }
    
    pthread_mutex_unlock(&modbus_mutex);
    
    // Determinar sucesso baseado na confiança e tamanho da placa
    result->success = (result->confidence >= MIN_PLATE_CONFIDENCE && 
                       strlen(result->plate) >= 7);
    
    const char* status_str = result->success ? "[OK]" : 
                             (result->confidence < LOW_PLATE_CONFIDENCE) ? 
                             "[CONFIANÇA MUITO BAIXA]" : "[BAIXA CONFIANÇA]";
    
    LOG_INFO("MODBUS", "Placa lida da câmera %s: '%s' (confiança: %d%%) %s", 
             camera_name, result->plate, result->confidence, status_str);
    
    return 0;
}

int modbus_camera_capture_and_read(camera_type_t camera, plate_reading_t* result) {
    if (!result) return -1;
    
    // Disparar captura
    if (modbus_camera_trigger(camera) != 0) {
        return -1;
    }
    
    // Aguardar e ler resultado
    return modbus_camera_read_plate(camera, result, 2000);
}

int modbus_camera_get_status(camera_type_t camera) {
    if (!initialized || !ctx) {
        return -1;
    }
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, camera);
    
    uint16_t status_reg;
    if (modbus_read_registers(ctx, LPR_REG_STATUS, 1, &status_reg) != 1) {
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    pthread_mutex_unlock(&modbus_mutex);
    return (int)status_reg;
}

int modbus_camera_reset(camera_type_t camera) {
    if (!initialized || !ctx) {
        return -1;
    }
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, camera);
    
    // Escrever 0 no trigger para resetar
    if (modbus_write_register(ctx, LPR_REG_TRIGGER, 0) != 1) {
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    pthread_mutex_unlock(&modbus_mutex);
    
    LOG_INFO("MODBUS", "Câmera 0x%02X resetada", camera);
    return 0;
}

// =============================================================================
// FUNÇÕES PÚBLICAS - DISPLAY
// =============================================================================

int modbus_display_update(const display_info_t* info) {
    if (!initialized || !ctx || !info) {
        LOG_ERROR("MODBUS", "Parâmetros inválidos");
        return -1;
    }
    
    LOG_DEBUG("MODBUS", "Atualizando placar");
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    // Preparar dados (13 registros)
    uint16_t data[13];
    data[0] = info->terreo_pne;
    data[1] = info->terreo_idoso;
    data[2] = info->terreo_comum;
    data[3] = info->andar1_pne;
    data[4] = info->andar1_idoso;
    data[5] = info->andar1_comum;
    data[6] = info->andar2_pne;
    data[7] = info->andar2_idoso;
    data[8] = info->andar2_comum;
    data[9] = info->total_pne;
    data[10] = info->total_idoso;
    data[11] = info->total_comum;
    
    // Flags
    data[12] = 0;
    if (info->lotado_geral) data[12] |= DISPLAY_FLAG_LOTADO_GERAL;
    if (info->bloqueado_andar1) data[12] |= DISPLAY_FLAG_BLOQ_ANDAR1;
    if (info->bloqueado_andar2) data[12] |= DISPLAY_FLAG_BLOQ_ANDAR2;
    
    // Preparar mensagem Write Multiple Registers (0x10)
    uint8_t req[256];
    int req_length = 0;
    
    req[req_length++] = MODBUS_ADDR_DISPLAY;  // Slave
    req[req_length++] = 0x10;                 // Function code
    req[req_length++] = 0x00;                 // Start address HIGH
    req[req_length++] = 0x00;                 // Start address LOW
    req[req_length++] = 0x00;                 // Quantity HIGH (13)
    req[req_length++] = 0x0D;                 // Quantity LOW
    req[req_length++] = 0x1A;                 // Byte count (13*2 = 26)
    
    // Dados dos registros
    for (int i = 0; i < 13; i++) {
        req[req_length++] = (data[i] >> 8) & 0xFF;
        req[req_length++] = data[i] & 0xFF;
    }
    
    // Adicionar matrícula antes do CRC
    req_length = add_matricula_to_message(req, req_length);
    
    // Calcular e adicionar CRC
    uint16_t crc = modbus_crc16(req, req_length);
    req[req_length++] = crc & 0xFF;
    req[req_length++] = (crc >> 8) & 0xFF;
    
    stats.requests_sent++;
    
    // Enviar requisição
    int rc = modbus_send_raw_request(ctx, req, req_length);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao enviar atualização do placar: %s", 
                  modbus_strerror(errno));
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    // Receber confirmação
    uint8_t rsp[MODBUS_RTU_MAX_ADU_LENGTH];
    rc = modbus_receive_confirmation(ctx, rsp);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao receber confirmação do placar: %s",
                  modbus_strerror(errno));
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    stats.responses_received++;
    pthread_mutex_unlock(&modbus_mutex);
    
    LOG_INFO("MODBUS", "Placar atualizado: T=%d/%d/%d A1=%d/%d/%d A2=%d/%d/%d", 
             data[0], data[1], data[2], data[3], data[4], data[5],
             data[6], data[7], data[8]);
    
    return 0;
}

int modbus_display_update_floor(int floor, uint8_t pne, uint8_t idoso, uint8_t comum) {
    if (!initialized || !ctx || floor < 0 || floor > 2) {
        return -1;
    }
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    // Calcular offset baseado no andar
    int offset = floor * 3; // 0, 3 ou 6
    
    uint16_t data[3] = {pne, idoso, comum};
    
    if (modbus_write_registers(ctx, offset, 3, data) != 3) {
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    stats.requests_sent++;
    stats.responses_received++;
    pthread_mutex_unlock(&modbus_mutex);
    
    return 0;
}

int modbus_display_update_flags(bool lotado, bool bloq_andar1, bool bloq_andar2) {
    if (!initialized || !ctx) {
        return -1;
    }
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    uint16_t flags = 0;
    if (lotado) flags |= DISPLAY_FLAG_LOTADO_GERAL;
    if (bloq_andar1) flags |= DISPLAY_FLAG_BLOQ_ANDAR1;
    if (bloq_andar2) flags |= DISPLAY_FLAG_BLOQ_ANDAR2;
    
    if (modbus_write_register(ctx, DISPLAY_REG_FLAGS, flags) != 1) {
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    stats.requests_sent++;
    stats.responses_received++;
    pthread_mutex_unlock(&modbus_mutex);
    
    return 0;
}

int modbus_display_read(display_info_t* info) {
    if (!initialized || !ctx || !info) {
        return -1;
    }
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    uint16_t regs[13];
    if (modbus_read_registers(ctx, 0, 13, regs) != 13) {
        stats.errors++;
        pthread_mutex_unlock(&modbus_mutex);
        return -1;
    }
    
    stats.requests_sent++;
    stats.responses_received++;
    
    info->terreo_pne = regs[0];
    info->terreo_idoso = regs[1];
    info->terreo_comum = regs[2];
    info->andar1_pne = regs[3];
    info->andar1_idoso = regs[4];
    info->andar1_comum = regs[5];
    info->andar2_pne = regs[6];
    info->andar2_idoso = regs[7];
    info->andar2_comum = regs[8];
    info->total_pne = regs[9];
    info->total_idoso = regs[10];
    info->total_comum = regs[11];
    
    info->lotado_geral = (regs[12] & DISPLAY_FLAG_LOTADO_GERAL) != 0;
    info->bloqueado_andar1 = (regs[12] & DISPLAY_FLAG_BLOQ_ANDAR1) != 0;
    info->bloqueado_andar2 = (regs[12] & DISPLAY_FLAG_BLOQ_ANDAR2) != 0;
    
    pthread_mutex_unlock(&modbus_mutex);
    
    return 0;
}

// =============================================================================
// FUNÇÕES PÚBLICAS - DIAGNÓSTICO
// =============================================================================

int modbus_test_device(uint8_t address) {
    if (!initialized || !ctx) {
        return -1;
    }
    
    LOG_INFO("MODBUS", "Testando dispositivo 0x%02X...", address);
    
    pthread_mutex_lock(&modbus_mutex);
    modbus_set_slave(ctx, address);
    
    uint16_t reg;
    int rc = modbus_read_registers(ctx, 0, 1, &reg);
    
    pthread_mutex_unlock(&modbus_mutex);
    
    if (rc == 1) {
        LOG_INFO("MODBUS", "  ✓ Dispositivo 0x%02X responde", address);
        return 0;
    } else {
        LOG_ERROR("MODBUS", "  ✗ Dispositivo 0x%02X não responde", address);
        return -1;
    }
}

int modbus_test_all_devices(void) {
    if (!initialized) {
        LOG_ERROR("MODBUS", "Cliente não inicializado");
        return -1;
    }
    
    LOG_INFO("MODBUS", "Testando todos os dispositivos...");
    
    int success_count = 0;
    
    // Testar câmera de entrada
    if (modbus_test_device(MODBUS_ADDR_CAMERA_ENTRADA) == 0) {
        success_count++;
    }
    
    // Testar câmera de saída
    if (modbus_test_device(MODBUS_ADDR_CAMERA_SAIDA) == 0) {
        success_count++;
    }
    
    // Testar placar
    if (modbus_test_device(MODBUS_ADDR_DISPLAY) == 0) {
        success_count++;
    }
    
    LOG_INFO("MODBUS", "Teste concluído: %d/3 dispositivos responderam", success_count);
    
    return (success_count == 3) ? 0 : -1;
}

void modbus_get_stats(modbus_stats_t* stats_out) {
    if (!stats_out) return;
    
    pthread_mutex_lock(&modbus_mutex);
    memcpy(stats_out, &stats, sizeof(modbus_stats_t));
    pthread_mutex_unlock(&modbus_mutex);
}

void modbus_reset_stats(void) {
    pthread_mutex_lock(&modbus_mutex);
    memset(&stats, 0, sizeof(modbus_stats_t));
    pthread_mutex_unlock(&modbus_mutex);
    
    LOG_INFO("MODBUS", "Estatísticas resetadas");
}

void modbus_print_diagnostics(void) {
    pthread_mutex_lock(&modbus_mutex);
    
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           DIAGNÓSTICO MODBUS                                   ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    printf("║ Status: %-52s ║\n", initialized ? "INICIALIZADO" : "NÃO INICIALIZADO");
    printf("║                                                                ║\n");
    printf("║ ESTATÍSTICAS                                                   ║\n");
    printf("║   Requisições enviadas:    %-32u║\n", stats.requests_sent);
    printf("║   Respostas recebidas:     %-32u║\n", stats.responses_received);
    printf("║   Erros:                   %-32u║\n", stats.errors);
    printf("║   Timeouts:                %-32u║\n", stats.timeouts);
    printf("║   Erros CRC:               %-32u║\n", stats.crc_errors);
    printf("║                                                                ║\n");
    
    if (stats.requests_sent > 0) {
        float success_rate = (float)stats.responses_received / stats.requests_sent * 100.0f;
        printf("║   Taxa de sucesso:         %.1f%%                             ║\n", success_rate);
    }
    
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    pthread_mutex_unlock(&modbus_mutex);
}

// =============================================================================
// FUNÇÕES PÚBLICAS - CONFIGURAÇÃO
// =============================================================================

void modbus_set_timeout(int timeout_ms) {
    if (timeout_ms < 10 || timeout_ms > 5000) {
        LOG_WARN("MODBUS", "Timeout inválido: %d ms (válido: 10-5000)", timeout_ms);
        return;
    }
    
    current_timeout_ms = timeout_ms;
    
    if (initialized && ctx) {
        struct timeval timeout;
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        modbus_set_response_timeout(ctx, timeout.tv_sec, timeout.tv_usec);
    }
    
    LOG_INFO("MODBUS", "Timeout configurado: %d ms", timeout_ms);
}

void modbus_set_retries(int retries) {
    if (retries < 0 || retries > 10) {
        LOG_WARN("MODBUS", "Número de retries inválido: %d (válido: 0-10)", retries);
        return;
    }
    
    current_retries = retries;
    LOG_INFO("MODBUS", "Retries configurados: %d", retries);
}

void modbus_set_debug(bool enable) {
    debug_enabled = enable;
    
    if (initialized && ctx) {
        modbus_set_debug(ctx, enable ? TRUE : FALSE);
    }
    
    LOG_INFO("MODBUS", "Debug MODBUS: %s", enable ? "ATIVADO" : "DESATIVADO");
}