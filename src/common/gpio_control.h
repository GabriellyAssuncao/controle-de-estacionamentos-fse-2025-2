/**
 * @file gpio_control.h
 * @brief Header do módulo de controle GPIO
 */

#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include "parking_system.h"

/**
 * @brief Inicializa o sistema GPIO da Raspberry Pi
 * @return 0 se sucesso, -1 se erro
 */
int gpio_init(void);

/**
 * @brief Faz cleanup e libera recursos do GPIO
 */
void gpio_cleanup(void);

/**
 * @brief Configura o endereço para multiplexação (0-7)
 * @param config Configuração do andar
 * @param address Endereço da vaga (0-7)
 * @return 0 se sucesso, -1 se erro
 */
int gpio_set_address(const gpio_floor_config_t* config, uint8_t address);

/**
 * @brief Lê o estado do sensor de vaga no endereço atual
 * @param config Configuração do andar
 * @return true se vaga ocupada, false se livre
 */
bool gpio_read_parking_sensor(const gpio_floor_config_t* config);

/**
 * @brief Lê o estado de um sensor de cancela
 * @param pin Pino GPIO do sensor
 * @return true se sensor ativo, false se inativo
 */
bool gpio_read_gate_sensor(uint8_t pin);

/**
 * @brief Controla o motor da cancela
 * @param pin Pino GPIO do motor
 * @param activate true para ativar, false para desativar
 */
void gpio_set_gate_motor(uint8_t pin, bool activate);

/**
 * @brief Função de teste para validar todos os pinos
 */
void gpio_test_all_pins(void);

#endif // GPIO_CONTROL_H