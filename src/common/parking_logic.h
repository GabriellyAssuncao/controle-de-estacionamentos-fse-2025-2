/**
 * @file parking_logic.h
 * @brief Header da lógica de negócio do estacionamento
 */

#ifndef PARKING_LOGIC_H
#define PARKING_LOGIC_H

#include "parking_system.h"

/**
 * @brief Inicializa o sistema de estacionamento
 * @param status Estrutura de status a ser inicializada
 */
void parking_init(parking_status_t* status);

/**
 * @brief Faz varredura de um andar específico
 * @param floor_id ID do andar
 * @param config Configuração GPIO do andar
 * @param floor_status Status do andar a ser atualizado
 * @return Número de mudanças detectadas, -1 se erro
 */
int parking_scan_floor(floor_id_t floor_id, const gpio_floor_config_t* config,
                       floor_status_t* floor_status);

/**
 * @brief Aloca uma vaga para um veículo
 * @param status Status do sistema
 * @param plate Placa do veículo
 * @param preferred_floor Andar preferido
 * @return true se alocado com sucesso
 */
bool parking_allocate_spot(parking_status_t* status, const char* plate, floor_id_t preferred_floor);

/**
 * @brief Libera a vaga de um veículo
 * @param status Status do sistema
 * @param plate Placa do veículo
 * @return true se liberado com sucesso
 */
bool parking_free_spot(parking_status_t* status, const char* plate);

/**
 * @brief Calcula a tarifa de estacionamento
 * @param entry_time Horário de entrada
 * @param exit_time Horário de saída
 * @return Valor em centavos
 */
uint32_t parking_calculate_fee(time_t entry_time, time_t exit_time);

/**
 * @brief Atualiza estatísticas totais do sistema
 * @param status Status do sistema
 */
void parking_update_total_stats(parking_status_t* status);

/**
 * @brief Bloqueia/desbloqueia um andar
 * @param status Status do sistema
 * @param floor_id ID do andar
 * @param blocked true para bloquear, false para desbloquear
 */
void parking_set_floor_blocked(parking_status_t* status, floor_id_t floor_id, bool blocked);

/**
 * @brief Ativa/desativa modo de emergência
 * @param status Status do sistema
 * @param emergency true para ativar, false para desativar
 */
void parking_set_emergency_mode(parking_status_t* status, bool emergency);

/**
 * @brief Imprime status completo do sistema
 * @param status Status do sistema
 */
void parking_print_status(const parking_status_t* status);

#endif // PARKING_LOGIC_H