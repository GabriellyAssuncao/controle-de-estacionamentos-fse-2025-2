/**
 * @file parking_system.h
 * @brief Header principal do sistema de controle de estacionamento
 */

#ifndef PARKING_SYSTEM_H
#define PARKING_SYSTEM_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "system_config.h"

// =============================================================================
// DEFINIÇÕES DE TIPOS
// =============================================================================

typedef struct {
    char plate[9];        // Placa do veículo (8 chars + \0)
    time_t entry_time;    // Timestamp de entrada
    time_t exit_time;     // Timestamp de saída
    floor_id_t floor;     // Andar onde está estacionado
    uint8_t spot;         // Número da vaga (0-7)
    int confidence;       // Confiança da leitura da placa
    bool is_anonymous;    // Se é um ticket anônimo
    uint32_t ticket_id;   // ID do ticket (para casos anônimos)
} vehicle_record_t;

typedef struct {
    bool occupied;        // Se a vaga está ocupada
    char plate[9];        // Placa do veículo (se conhecido)
    time_t timestamp;     // Timestamp da última mudança
} parking_spot_t;

typedef struct {
    parking_spot_t spots[MAX_PARKING_SPOTS_PER_FLOOR];
    uint8_t free_spots;   // Número de vagas livres
    bool blocked;         // Se o andar está bloqueado
} floor_status_t;

typedef struct {
    floor_status_t floors[MAX_FLOORS];
    uint8_t total_free_spots;
    bool system_full;     // Se o estacionamento está lotado
    bool emergency_mode;  // Modo de emergência
} parking_status_t;

// =============================================================================
// MENSAGENS JSON
// =============================================================================

typedef enum {
    MSG_TYPE_ENTRY_OK = 1,
    MSG_TYPE_EXIT_OK,
    MSG_TYPE_PARKING_STATUS,
    MSG_TYPE_VEHICLE_DETECTED,
    MSG_TYPE_GATE_COMMAND,
    MSG_TYPE_SYSTEM_STATUS,
    MSG_TYPE_ERROR
} message_type_t;

typedef struct {
    message_type_t type;
    time_t timestamp;
    union {
        struct {
            char plate[9];
            int confidence;
            floor_id_t floor;
        } vehicle_event;
        
        struct {
            uint8_t free_spots_terreo;
            uint8_t free_spots_andar1;
            uint8_t free_spots_andar2;
            uint8_t free_spots_total;
            bool is_full;
            bool floor2_blocked;
        } parking_status;
        
        struct {
            bool open_gate;
            bool is_entry;  // true = entrada, false = saída
        } gate_command;
        
        struct {
            int error_code;
            char description[256];
        } error_info;
    } data;
} system_message_t;

// =============================================================================
// PROTÓTIPOS DE FUNÇÕES (serão implementadas nos módulos específicos)
// =============================================================================

// GPIO Control
int gpio_init(void);
void gpio_cleanup(void);
int gpio_set_address(const gpio_floor_config_t* config, uint8_t address);
bool gpio_read_parking_sensor(const gpio_floor_config_t* config);
bool gpio_read_gate_sensor(uint8_t pin);
void gpio_set_gate_motor(uint8_t pin, bool activate);

// MODBUS Client
int modbus_init(const char* device, int baudrate);
void modbus_cleanup(void);
int modbus_trigger_camera(uint8_t device_addr);
int modbus_read_plate(uint8_t device_addr, char* plate, int* confidence);
int modbus_update_display(uint8_t free_terreo, uint8_t free_andar1, 
                          uint8_t free_andar2, uint8_t free_total, uint16_t flags);

// TCP Communication
int tcp_server_init(int port);
int tcp_client_connect(const char* host, int port);
int tcp_send_message(int socket, const system_message_t* msg);
int tcp_receive_message(int socket, system_message_t* msg);
void tcp_close_connection(int socket);

// Parking Logic
void parking_init(parking_status_t* status);
int parking_scan_floor(floor_id_t floor_id, const gpio_floor_config_t* config,
                       floor_status_t* floor_status);
bool parking_allocate_spot(parking_status_t* status, const char* plate, floor_id_t preferred_floor);
bool parking_free_spot(parking_status_t* status, const char* plate);
uint32_t parking_calculate_fee(time_t entry_time, time_t exit_time);

// System Logger
int logger_init(const char* log_dir);
void logger_cleanup(void);
void logger_log(log_level_t level, const char* module, const char* format, ...);

// Macros de logging
#define LOG_DEBUG(module, ...) logger_log(LOG_LEVEL_DEBUG, module, __VA_ARGS__)
#define LOG_INFO(module, ...)  logger_log(LOG_LEVEL_INFO, module, __VA_ARGS__)
#define LOG_WARN(module, ...)  logger_log(LOG_LEVEL_WARNING, module, __VA_ARGS__)
#define LOG_ERROR(module, ...) logger_log(LOG_LEVEL_ERROR, module, __VA_ARGS__)
#define LOG_FATAL(module, ...) logger_log(LOG_LEVEL_FATAL, module, __VA_ARGS__)

// =============================================================================
// UTILITÁRIOS
// =============================================================================

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// Conversão de tempo para string
static inline void time_to_string(time_t t, char* buffer, size_t size) {
    struct tm* tm_info = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

// Validação de placa (formato brasileiro simplificado)
static inline bool is_valid_plate(const char* plate) {
    return strlen(plate) >= 7 && strlen(plate) <= 8;
}

#endif // PARKING_SYSTEM_H