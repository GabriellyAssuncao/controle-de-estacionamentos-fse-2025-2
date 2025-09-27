/**
 * @file modbus_client.h
 * @brief Header do cliente MODBUS
 */

#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include "parking_system.h"

/**
 * @brief Inicializa cliente MODBUS
 * @param device Dispositivo serial (ex: "/dev/ttyUSB0")
 * @param baudrate Taxa de transmissão
 * @return 0 se sucesso, -1 se erro
 */
int modbus_init(const char* device, int baudrate);

/**
 * @brief Finaliza cliente MODBUS
 */
void modbus_cleanup(void);

/**
 * @brief Dispara captura na câmera LPR
 * @param device_addr Endereço MODBUS da câmera
 * @return 0 se sucesso, -1 se erro
 */
int modbus_trigger_camera(uint8_t device_addr);

/**
 * @brief Lê placa capturada da câmera
 * @param device_addr Endereço MODBUS da câmera
 * @param plate Buffer para a placa (9 bytes)
 * @param confidence Confiança da leitura (0-100)
 * @return 0 se sucesso, -1 se erro
 */
int modbus_read_plate(uint8_t device_addr, char* plate, int* confidence);

/**
 * @brief Atualiza display do placar
 * @param free_terreo Vagas livres térreo
 * @param free_andar1 Vagas livres 1º andar
 * @param free_andar2 Vagas livres 2º andar
 * @param free_total Total de vagas livres
 * @param flags Flags de status
 * @return 0 se sucesso, -1 se erro
 */
int modbus_update_display(uint8_t free_terreo, uint8_t free_andar1, 
                          uint8_t free_andar2, uint8_t free_total, uint16_t flags);

#endif // MODBUS_CLIENT_H