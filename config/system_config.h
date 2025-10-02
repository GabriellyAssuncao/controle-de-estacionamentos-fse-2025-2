/**
 * @file system_config.h
 * @brief Configurações do sistema de controle de estacionamento
 */

#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

// =============================================================================
// CONFIGURAÇÕES GERAIS DO SISTEMA
// =============================================================================

#define SYSTEM_VERSION "1.0"
#define MAX_FLOORS 3
#define MAX_PARKING_SPOTS_PER_FLOOR 8
#define TOTAL_PARKING_SPOTS (MAX_FLOORS * MAX_PARKING_SPOTS_PER_FLOOR)

// Preço por minuto (em centavos para evitar ponto flutuante)
#define PRICE_PER_MINUTE_CENTS 15  // R$ 0,15

// =============================================================================
// CONFIGURAÇÕES DE REDE TCP/IP
// =============================================================================

#define SERVER_CENTRAL_PORT 8080
#define SERVER_TERREO_PORT 8081
#define SERVER_ANDAR1_PORT 8082
#define SERVER_ANDAR2_PORT 8083

#define SERVER_CENTRAL_IP "127.0.0.1"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

// Timeouts de rede (em segundos)
#define TCP_CONNECT_TIMEOUT 5
#define TCP_RECEIVE_TIMEOUT 10

// =============================================================================
// CONFIGURAÇÕES MODBUS
// =============================================================================

#define MODBUS_DEVICE "/dev/ttyUSB0"
#define MODBUS_BAUDRATE 115200
#define MODBUS_TIMEOUT_MS 500
#define MODBUS_MAX_RETRIES 3

// Endereços dos dispositivos MODBUS
#define MODBUS_ADDR_CAMERA_IN 0x11
#define MODBUS_ADDR_CAMERA_OUT 0x12
#define MODBUS_ADDR_DISPLAY_BOARD 0x20

// Registros das câmeras LPR
#define LPR_REG_STATUS 0
#define LPR_REG_TRIGGER 1
#define LPR_REG_PLATE_START 2
#define LPR_REG_PLATE_SIZE 4
#define LPR_REG_CONFIDENCE 6
#define LPR_REG_ERROR 7

// Registros do placar
#define DISPLAY_REG_SPOTS_TERREO 0
#define DISPLAY_REG_SPOTS_ANDAR1 1
#define DISPLAY_REG_SPOTS_ANDAR2 2
#define DISPLAY_REG_SPOTS_TOTAL 3
#define DISPLAY_REG_FLAGS 4

// Status da câmera LPR
#define LPR_STATUS_READY 0
#define LPR_STATUS_PROCESSING 1
#define LPR_STATUS_OK 2
#define LPR_STATUS_ERROR 3

// Confiança mínima para aceitar uma placa
#define MIN_PLATE_CONFIDENCE 70

// =============================================================================
// CONFIGURAÇÕES GPIO - ANDAR TÉRREO
// =============================================================================

#define GPIO_TERREO_ENDERECO_01 17
#define GPIO_TERREO_ENDERECO_02 18
#define GPIO_TERREO_ENDERECO_03 23
#define GPIO_TERREO_SENSOR_VAGA 8
#define GPIO_TERREO_SENSOR_ABERTURA_ENTRADA 7
#define GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA 1
#define GPIO_TERREO_MOTOR_ENTRADA 24
#define GPIO_TERREO_SENSOR_ABERTURA_SAIDA 12
#define GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA 16  // XX na especificação
#define GPIO_TERREO_MOTOR_SAIDA 25

// =============================================================================
// CONFIGURAÇÕES GPIO - 1º ANDAR
// =============================================================================

#define GPIO_ANDAR1_ENDERECO_01 16
#define GPIO_ANDAR1_ENDERECO_02 20
#define GPIO_ANDAR1_ENDERECO_03 21
#define GPIO_ANDAR1_SENSOR_VAGA 27
#define GPIO_ANDAR1_SENSOR_PASSAGEM_1 22
#define GPIO_ANDAR1_SENSOR_PASSAGEM_2 11

// =============================================================================
// CONFIGURAÇÕES GPIO - 2º ANDAR
// =============================================================================

#define GPIO_ANDAR2_ENDERECO_01 0
#define GPIO_ANDAR2_ENDERECO_02 5
#define GPIO_ANDAR2_ENDERECO_03 6
#define GPIO_ANDAR2_SENSOR_VAGA 13
#define GPIO_ANDAR2_SENSOR_PASSAGEM_1 19
#define GPIO_ANDAR2_SENSOR_PASSAGEM_2 26

// =============================================================================
// CONFIGURAÇÕES DE TIMING
// =============================================================================

#define GPIO_SCAN_INTERVAL_MS 100    // Intervalo de varredura das vagas
#define GATE_TIMEOUT_MS 5000         // Timeout para operação da cancela
#define MODBUS_POLL_INTERVAL_MS 100  // Intervalo de polling MODBUS
#define STATUS_UPDATE_INTERVAL_MS 1000  // Atualização do placar

// =============================================================================
// CONFIGURAÇÕES DE LOG
// =============================================================================

#define LOG_DIR "./logs"
#define LOG_FILE_MAX_SIZE_MB 10
#define LOG_FILE_MAX_COUNT 5

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_t;

#define DEFAULT_LOG_LEVEL LOG_LEVEL_INFO

// =============================================================================
// ESTRUTURAS DE DADOS PRINCIPAIS
// =============================================================================

typedef enum {
    FLOOR_TERREO = 0,
    FLOOR_ANDAR1 = 1,
    FLOOR_ANDAR2 = 2
} floor_id_t;

typedef enum {
    GATE_STATE_CLOSED = 0,
    GATE_STATE_OPENING,
    GATE_STATE_OPEN,
    GATE_STATE_CLOSING,
    GATE_STATE_ERROR
} gate_state_t;

typedef struct {
    uint8_t address_pins[3];  // Pinos de endereçamento
    uint8_t sensor_pin;       // Pino do sensor de vaga
} gpio_floor_config_t;

// Configurações de GPIO por andar
static const gpio_floor_config_t GPIO_CONFIGS[MAX_FLOORS] = {
    // Térreo
    {
        .address_pins = {GPIO_TERREO_ENDERECO_01, GPIO_TERREO_ENDERECO_02, GPIO_TERREO_ENDERECO_03},
        .sensor_pin = GPIO_TERREO_SENSOR_VAGA
    },
    // 1º Andar
    {
        .address_pins = {GPIO_ANDAR1_ENDERECO_01, GPIO_ANDAR1_ENDERECO_02, GPIO_ANDAR1_ENDERECO_03},
        .sensor_pin = GPIO_ANDAR1_SENSOR_VAGA
    },
    // 2º Andar
    {
        .address_pins = {GPIO_ANDAR2_ENDERECO_01, GPIO_ANDAR2_ENDERECO_02, GPIO_ANDAR2_ENDERECO_03},
        .sensor_pin = GPIO_ANDAR2_SENSOR_VAGA
    }
};

#endif // SYSTEM_CONFIG_H