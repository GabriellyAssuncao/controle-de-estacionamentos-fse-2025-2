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

 
typedef struct {
    bool occupied;         
    spot_type_t type;      
    char plate[9];         
    time_t timestamp;      
    int confidence;        
} parking_spot_t;

 typedef struct {
    parking_spot_t spots[MAX_PARKING_SPOTS_PER_FLOOR];
    uint8_t num_spots; 
    
    uint8_t free_pne;      
    uint8_t free_idoso;    
    uint8_t free_comum;    
    uint8_t total_free;        
    uint8_t cars_count;    
    bool blocked;          
} floor_status_t;

typedef struct {
    floor_status_t floors[MAX_FLOORS];
    
    uint8_t total_free_pne;
    uint8_t total_free_idoso;
    uint8_t total_free_comum;
    uint8_t total_free_spots;
    uint8_t total_cars;
    
    bool system_full;      
    bool emergency_mode;   
} parking_status_t;

 typedef struct {
    char plate[9];         
    time_t entry_time;      
    time_t exit_time;      
    floor_id_t floor;      
    uint8_t spot;          
    int confidence;        
    bool is_anonymous;     
    uint32_t ticket_id;    
    bool paid;             
    uint32_t amount_cents; 
} vehicle_record_t;
 
typedef enum {
    MSG_TYPE_ENTRY_OK = 1,
    MSG_TYPE_EXIT_OK,
    MSG_TYPE_PARKING_STATUS,
    MSG_TYPE_VEHICLE_DETECTED,
    MSG_TYPE_GATE_COMMAND,
    MSG_TYPE_SYSTEM_STATUS,
    MSG_TYPE_PASSAGE_DETECTED,   
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
             
            uint8_t terreo_pne, terreo_idoso, terreo_comum;
            uint8_t andar1_pne, andar1_idoso, andar1_comum;
            uint8_t andar2_pne, andar2_idoso, andar2_comum;
            
             
            uint8_t cars_terreo, cars_andar1, cars_andar2;
            
             
            bool lotado_geral;
            bool lotado_andar1;
            bool lotado_andar2;
        } parking_status;
        
        struct {
            floor_id_t from_floor;
            floor_id_t to_floor;
            char plate[9];   
        } passage;
        
        struct {
            bool open_gate;
            bool is_entry;   
        } gate_command;
        
        struct {
            int error_code;
            char description[256];
        } error_info;
    } data;
} system_message_t;
 
int gpio_init(void);
void gpio_cleanup(void);
int gpio_set_address(const gpio_floor_config_t* config, uint8_t address);
bool gpio_read_parking_sensor(const gpio_floor_config_t* config);
bool gpio_read_gate_sensor(uint8_t pin);
void gpio_set_gate_motor(uint8_t pin, bool activate);

 
int modbus_init(const char* device, int baudrate);
void modbus_cleanup(void);
int modbus_camera_trigger(uint8_t camera_addr);
int modbus_camera_read_plate(uint8_t camera_addr, char* plate, int* confidence, int timeout_ms);
int modbus_display_update_full(const parking_status_t* status);
int modbus_display_update_floor(floor_id_t floor, uint8_t pne, uint8_t idoso, uint8_t comum);

int tcp_server_init(int port);
int tcp_client_connect(const char* host, int port);
int tcp_send_message(int socket, const system_message_t* msg);
int tcp_receive_message(int socket, system_message_t* msg);
void tcp_close_connection(int socket);

void parking_init(parking_status_t* status);
int parking_scan_floor(floor_id_t floor_id, const gpio_floor_config_t* config,
                       floor_status_t* floor_status);
bool parking_allocate_spot(parking_status_t* status, const char* plate, 
                           spot_type_t preferred_type, floor_id_t preferred_floor);
bool parking_free_spot(parking_status_t* status, const char* plate);
uint32_t parking_calculate_fee(time_t entry_time, time_t exit_time);
void parking_update_total_stats(parking_status_t* status);

int logger_init(const char* log_dir);
void logger_cleanup(void);
void logger_log(log_level_t level, const char* module, const char* format, ...);

#define LOG_DEBUG(module, ...) logger_log(LOG_LEVEL_DEBUG, module, __VA_ARGS__)
#define LOG_INFO(module, ...)  logger_log(LOG_LEVEL_INFO, module, __VA_ARGS__)
#define LOG_WARN(module, ...)  logger_log(LOG_LEVEL_WARNING, module, __VA_ARGS__)
#define LOG_ERROR(module, ...) logger_log(LOG_LEVEL_ERROR, module, __VA_ARGS__)
#define LOG_FATAL(module, ...) logger_log(LOG_LEVEL_FATAL, module, __VA_ARGS__)

 
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static inline void time_to_string(time_t t, char* buffer, size_t size) {
    struct tm* tm_info = localtime(&t);
    strftime(buffer, size, "%Y-%m-%d %H:%M:%S", tm_info);
}

static inline bool is_valid_plate(const char* plate) {
    return strlen(plate) >= 7 && strlen(plate) <= 8;
}

static inline const char* spot_type_to_string(spot_type_t type) {
    switch(type) {
        case SPOT_TYPE_PNE: return "PNE";
        case SPOT_TYPE_IDOSO: return "IDOSO+";
        case SPOT_TYPE_COMUM: return "COMUM";
        default: return "DESCONHECIDO";
    }
}

 static inline void format_money(uint32_t cents, char* buffer, size_t size) {
    snprintf(buffer, size, "R$ %u,%02u", cents / 100, cents % 100);
}

#endif  