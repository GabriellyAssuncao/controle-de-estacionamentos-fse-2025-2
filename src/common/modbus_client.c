#include "modbus_client.h"
#include "system_logger.h"
#include <modbus/modbus.h>
#include <string.h>

static int add_matricula_to_message(uint8_t* buffer, int length) {
    const char* matricula = MODBUS_MATRICULA;
    size_t mat_len = strlen(matricula);
    
    if (mat_len < 4) {
        LOG_ERROR("MODBUS", "Matrícula inválida: deve ter pelo menos 4 dígitos");
        return length;
    }
    
    const char* last_4 = matricula + (mat_len - 4);
    
    uint16_t mat_part1 = ((last_4[0] - '0') << 8) | (last_4[1] - '0');
    uint16_t mat_part2 = ((last_4[2] - '0') << 8) | (last_4[3] - '0');
    
    buffer[length++] = (mat_part1 >> 8) & 0xFF;
    buffer[length++] = mat_part1 & 0xFF;
    buffer[length++] = (mat_part2 >> 8) & 0xFF;
    buffer[length++] = mat_part2 & 0xFF;
    
    LOG_DEBUG("MODBUS", "Matrícula adicionada à mensagem: %c%c%c%c", 
              last_4[0], last_4[1], last_4[2], last_4[3]);
    
    return length;
}

int modbus_camera_trigger(camera_type_t camera) {
    if (!initialized || !ctx) {
        LOG_ERROR("MODBUS", "Cliente não inicializado");
        return -1;
    }
    
    LOG_DEBUG("MODBUS", "Disparando câmera 0x%02X", camera);
    
    modbus_set_slave(ctx, camera);
    
    uint8_t req[256];
    int req_length;
    
    req[0] = camera;
    req[1] = 0x06;
    req[2] = (LPR_REG_TRIGGER >> 8);
    req[3] = (LPR_REG_TRIGGER & 0xFF);
    req[4] = 0x00;
    req[5] = 0x01;
    req_length = 6;
    
    req_length = add_matricula_to_message(req, req_length);
    
    uint16_t crc = modbus_crc16(req, req_length);
    req[req_length++] = crc & 0xFF;
    req[req_length++] = (crc >> 8) & 0xFF;
    
    stats.requests_sent++;
    
    int rc = modbus_send_raw_request(ctx, req, req_length);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao disparar câmera 0x%02X: %s", 
                  camera, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    uint8_t rsp[MODBUS_RTU_MAX_ADU_LENGTH];
    rc = modbus_receive_confirmation(ctx, rsp);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao receber confirmação da câmera 0x%02X: %s",
                  camera, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    LOG_INFO("MODBUS", "Câmera 0x%02X disparada com sucesso", camera);
    return 0;
}

int modbus_camera_read_plate_with_matricula(camera_type_t camera, 
                                            plate_reading_t* result, 
                                            int timeout_ms) {
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
    LOG_DEBUG("MODBUS", "Lendo placa da câmera %s (timeout: %dms)", 
              camera_name, timeout_ms);
    
    modbus_set_slave(ctx, camera);
    
    int elapsed = 0;
    const int poll_interval = 100;
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
                return -1;
            }
        }
        
        usleep(poll_interval * 1000);
        elapsed += poll_interval;
    }
    
    if (status != LPR_STATUS_OK) {
        LOG_ERROR("MODBUS", "Timeout aguardando câmera %s", camera_name);
        stats.timeouts++;
        return -1;
    }
    
    uint16_t plate_regs[4];
    stats.requests_sent++;
    
    if (modbus_read_registers(ctx, LPR_REG_PLATE, 4, plate_regs) != 4) {
        LOG_ERROR("MODBUS", "Erro ao ler placa da câmera %s: %s", 
                  camera_name, modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    
    for (int i = 0; i < 4; i++) {
        result->plate[i*2] = (plate_regs[i] >> 8) & 0xFF;
        result->plate[i*2 + 1] = plate_regs[i] & 0xFF;
    }
    result->plate[8] = '\0';
    
    for (int i = 0; i < 8; i++) {
        if (result->plate[i] < 32 || result->plate[i] > 126) {
            result->plate[i] = '\0';
            break;
        }
    }
    
    int len = strlen(result->plate);
    while (len > 0 && result->plate[len-1] == ' ') {
        result->plate[--len] = '\0';
    }
    
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
    
    result->success = (result->confidence >= MIN_PLATE_CONFIDENCE && 
                       strlen(result->plate) >= 7);
    
    LOG_INFO("MODBUS", "Placa lida da câmera %s: '%s' (confiança: %d%%) %s", 
             camera_name, result->plate, result->confidence,
             result->success ? "[OK]" : 
             result->confidence < LOW_PLATE_CONFIDENCE ? "[CONFIANÇA MUITO BAIXA]" : 
             "[BAIXA CONFIANÇA]");
    
    return 0;
}

int modbus_display_update_full(const parking_status_t* status) {
    if (!initialized || !ctx || !status) {
        LOG_ERROR("MODBUS", "Parâmetros inválidos");
        return -1;
    }
    
    LOG_DEBUG("MODBUS", "Atualizando placar completo");
    
    modbus_set_slave(ctx, MODBUS_ADDR_DISPLAY);
    
    uint16_t data[13];
    
    data[0] = status->floors[FLOOR_TERREO].free_pne;
    data[1] = status->floors[FLOOR_TERREO].free_idoso;
    data[2] = status->floors[FLOOR_TERREO].free_comum;
    
    data[3] = status->floors[FLOOR_ANDAR1].free_pne;
    data[4] = status->floors[FLOOR_ANDAR1].free_idoso;
    data[5] = status->floors[FLOOR_ANDAR1].free_comum;
    
    data[6] = status->floors[FLOOR_ANDAR2].free_pne;
    data[7] = status->floors[FLOOR_ANDAR2].free_idoso;
    data[8] = status->floors[FLOOR_ANDAR2].free_comum;
    
    data[9] = status->floors[FLOOR_TERREO].cars_count;
    data[10] = status->floors[FLOOR_ANDAR1].cars_count;
    data[11] = status->floors[FLOOR_ANDAR2].cars_count;
    
    data[12] = 0;
    if (status->system_full) data[12] |= DISPLAY_FLAG_LOTADO_GERAL;
    if (status->floors[FLOOR_ANDAR1].blocked || 
        status->floors[FLOOR_ANDAR1].total_free == 0) {
        data[12] |= DISPLAY_FLAG_LOTADO_ANDAR1;
    }
    if (status->floors[FLOOR_ANDAR2].blocked || 
        status->floors[FLOOR_ANDAR2].total_free == 0) {
        data[12] |= DISPLAY_FLAG_LOTADO_ANDAR2;
    }
    
    uint8_t req[256];
    int req_length = 0;
    
    req[req_length++] = MODBUS_ADDR_DISPLAY;
    req[req_length++] = 0x10;
    req[req_length++] = 0x00;
    req[req_length++] = 0x00;
    req[req_length++] = 0x00;
    req[req_length++] = 13;
    req[req_length++] = 13 * 2;
    
    for (int i = 0; i < 13; i++) {
        req[req_length++] = (data[i] >> 8) & 0xFF;
        req[req_length++] = data[i] & 0xFF;
    }
    
    req_length = add_matricula_to_message(req, req_length);
    
    uint16_t crc = modbus_crc16(req, req_length);
    req[req_length++] = crc & 0xFF;
    req[req_length++] = (crc >> 8) & 0xFF;
    
    stats.requests_sent++;
    
    int rc = modbus_send_raw_request(ctx, req, req_length);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao enviar atualização do placar: %s", 
                  modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    uint8_t rsp[MODBUS_RTU_MAX_ADU_LENGTH];
    rc = modbus_receive_confirmation(ctx, rsp);
    if (rc == -1) {
        LOG_ERROR("MODBUS", "Erro ao receber confirmação do placar: %s",
                  modbus_strerror(errno));
        stats.errors++;
        return -1;
    }
    
    stats.responses_received++;
    
    LOG_INFO("MODBUS", "Placar atualizado: T=%d/%d/%d A1=%d/%d/%d A2=%d/%d/%d Carros=%d/%d/%d", 
             data[0], data[1], data[2],
             data[3], data[4], data[5],
             data[6], data[7], data[8],
             data[9], data[10], data[11]);
    
    return 0