/**
 * @file gate_control.h
 * @brief Header do sistema de controle de cancelas
 */

#ifndef GATE_CONTROL_H
#define GATE_CONTROL_H

#include "parking_system.h"

/**
 * @brief Tipo de cancela
 */
typedef enum {
    GATE_ENTRY = 0,    // Cancela de entrada
    GATE_EXIT = 1      // Cancela de saída
} gate_type_t;

/**
 * @brief Inicializa o sistema de controle de cancelas
 * @return 0 se sucesso, -1 se erro
 */
int gate_system_init(void);

/**
 * @brief Finaliza o sistema de cancelas
 */
void gate_system_cleanup(void);

/**
 * @brief Abre uma cancela
 * @param gate_type Tipo da cancela
 * @return 0 se sucesso, -1 se erro
 */
int gate_open(gate_type_t gate_type);

/**
 * @brief Fecha uma cancela
 * @param gate_type Tipo da cancela
 * @return 0 se sucesso, -1 se erro
 */
int gate_close(gate_type_t gate_type);

/**
 * @brief Obtém o estado atual de uma cancela
 * @param gate_type Tipo da cancela
 * @return Estado atual da cancela
 */
gate_state_t gate_get_state(gate_type_t gate_type);

/**
 * @brief Reseta erro de uma cancela
 * @param gate_type Tipo da cancela
 * @return 0 se sucesso, -1 se erro
 */
int gate_reset_error(gate_type_t gate_type);

/**
 * @brief Abre todas as cancelas (emergência)
 */
void gate_emergency_open_all(void);

/**
 * @brief Imprime status das cancelas
 */
void gate_print_status(void);

#endif // GATE_CONTROL_H