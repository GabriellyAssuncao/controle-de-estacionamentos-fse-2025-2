/**
 * @file system_logger.h
 * @brief Header do sistema de logging
 */

#ifndef SYSTEM_LOGGER_H
#define SYSTEM_LOGGER_H

#include "parking_system.h"

/**
 * @brief Inicializa o sistema de logging
 * @param log_dir Diretório onde salvar os logs
 * @return 0 se sucesso, -1 se erro
 */
int logger_init(const char* log_dir);

/**
 * @brief Finaliza e libera recursos do logger
 */
void logger_cleanup(void);

/**
 * @brief Registra uma mensagem de log
 * @param level Nível do log
 * @param module Nome do módulo (ex: "GPIO", "MODBUS")
 * @param format String de formato (como printf)
 * @param ... Argumentos variáveis
 */
void logger_log(log_level_t level, const char* module, const char* format, ...);

/**
 * @brief Define o nível mínimo de log
 * @param level Nível mínimo
 */
void logger_set_level(log_level_t level);

/**
 * @brief Obtém o nível atual de log
 * @return Nível atual
 */
log_level_t logger_get_level(void);

#endif // SYSTEM_LOGGER_H