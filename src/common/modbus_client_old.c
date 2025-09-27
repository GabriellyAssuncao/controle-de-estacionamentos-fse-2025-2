/**
 * @file modbus_client.c
 * @brief Cliente MODBUS RS485 para comunicação com dispositivos externos
 * 
 * Este módulo implementa a comunicação MODBUS RTU para:
 * - Câmera LPR da entrada (0x11)
 * - Câmera LPR da saída (0x12)  
 * - Placar de vagas (0x20)
 */

#include "modbus_client.h"
#include "system_logger.h"

// Implementação temporária básica - será expandida depois
int modbus_init(const char* device, int baudrate) {
    (void)device;
    (void)baudrate;
    LOG_INFO("MODBUS", "MODBUS client inicializado (stub)");
    return 0;
}

void modbus_cleanup(void) {
    LOG_INFO("MODBUS", "MODBUS client finalizado (stub)");
}

int modbus_trigger_camera(uint8_t device_addr) {
    (void)device_addr;
    return 0;
}

int modbus_read_plate(uint8_t device_addr, char* plate, int* confidence) {
    (void)device_addr;
    (void)plate;
    (void)confidence;
    return 0;
}

int modbus_update_display(uint8_t free_terreo, uint8_t free_andar1, 
                          uint8_t free_andar2, uint8_t free_total, uint16_t flags) {
    (void)free_terreo;
    (void)free_andar1;
    (void)free_andar2;
    (void)free_total;
    (void)flags;
    return 0;
}