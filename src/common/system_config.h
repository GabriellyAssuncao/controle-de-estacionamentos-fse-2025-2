#ifndef SYSTEM_CONFIG_H
#define SYSTEM_CONFIG_H

#include <stdint.h>

#define SYSTEM_VERSION "1.0"
#define MAX_FLOORS 3

#define SPOTS_TERREO 4
#define SPOTS_ANDAR1 8
#define SPOTS_ANDAR2 8
#define MAX_PARKING_SPOTS_PER_FLOOR 8

#define TOTAL_PARKING_SPOTS (SPOTS_TERREO + SPOTS_ANDAR1 + SPOTS_ANDAR2)

typedef enum {
    SPOT_TYPE_PNE = 0,
    SPOT_TYPE_IDOSO = 1,
    SPOT_TYPE_COMUM = 2
} spot_type_t;

#define PRICE_PER_MINUTE_CENTS 15
#define MIN_PLATE_CONFIDENCE 70
#define LOW_PLATE_CONFIDENCE 60

#define SERVER_CENTRAL_PORT 8080
#define SERVER_TERREO_PORT 8081
#define SERVER_ANDAR1_PORT 8082
#define SERVER_ANDAR2_PORT 8083

#define SERVER_CENTRAL_IP "127.0.0.1"
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

#define TCP_CONNECT_TIMEOUT 5
#define TCP_RECEIVE_TIMEOUT 10

#define MODBUS_DEVICE "/dev/ttyUSB0"
#define MODBUS_BAUDRATE 115200
#define MODBUS_TIMEOUT_MS 500
#define MODBUS_MAX_RETRIES 3

#define MODBUS_ADDR_CAMERA_ENTRADA 0x11
#define MODBUS_ADDR_CAMERA_SAIDA   0x12
#define MODBUS_ADDR_DISPLAY        0x20

#define MODBUS_MATRICULA "1234"

#define LPR_REG_STATUS      0
#define LPR_REG_TRIGGER     1
#define LPR_REG_PLATE       2
#define LPR_REG_CONFIDENCE  6
#define LPR_REG_ERROR       7

#define LPR_STATUS_READY       0
#define LPR_STATUS_PROCESSING  1
#define LPR_STATUS_OK          2
#define LPR_STATUS_ERROR       3

#define DISPLAY_REG_TERREO_PNE      0
#define DISPLAY_REG_TERREO_IDOSO    1
#define DISPLAY_REG_TERREO_COMUM    2
#define DISPLAY_REG_ANDAR1_PNE      3
#define DISPLAY_REG_ANDAR1_IDOSO    4
#define DISPLAY_REG_ANDAR1_COMUM    5
#define DISPLAY_REG_ANDAR2_PNE      6
#define DISPLAY_REG_ANDAR2_IDOSO    7
#define DISPLAY_REG_ANDAR2_COMUM    8
#define DISPLAY_REG_CARROS_TERREO   9
#define DISPLAY_REG_CARROS_ANDAR1   10
#define DISPLAY_REG_CARROS_ANDAR2   11
#define DISPLAY_REG_FLAGS           12

#define DISPLAY_FLAG_LOTADO_GERAL   (1 << 0)
#define DISPLAY_FLAG_LOTADO_ANDAR1  (1 << 1)
#define DISPLAY_FLAG_LOTADO_ANDAR2  (1 << 2)

#define GPIO_TERREO_ENDERECO_01 17
#define GPIO_TERREO_ENDERECO_02 18
#define GPIO_TERREO_SENSOR_VAGA 8
#define GPIO_TERREO_SENSOR_ABERTURA_ENTRADA 7
#define GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA 1
#define GPIO_TERREO_MOTOR_ENTRADA 23
#define GPIO_TERREO_SENSOR_ABERTURA_SAIDA 12
#define GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA 25
#define GPIO_TERREO_MOTOR_SAIDA 24

#define GPIO_ANDAR1_ENDERECO_01 16
#define GPIO_ANDAR1_ENDERECO_02 20
#define GPIO_ANDAR1_ENDERECO_03 21
#define GPIO_ANDAR1_SENSOR_VAGA 27
#define GPIO_ANDAR1_SENSOR_PASSAGEM_1 22
#define GPIO_ANDAR1_SENSOR_PASSAGEM_2 11

#define GPIO_ANDAR2_ENDERECO_01 0
#define GPIO_ANDAR2_ENDERECO_02 5
#define GPIO_ANDAR2_ENDERECO_03 6
#define GPIO_ANDAR2_SENSOR_VAGA 13
#define GPIO_ANDAR2_SENSOR_PASSAGEM_1 19
#define GPIO_ANDAR2_SENSOR_PASSAGEM_2 26

#define GPIO_SCAN_INTERVAL_MS 100
#define GATE_TIMEOUT_MS 5000
#define MODBUS_POLL_INTERVAL_MS 100
#define STATUS_UPDATE_INTERVAL_MS 1000

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
    uint8_t address_pins[3];
    uint8_t num_address_bits;
    uint8_t sensor_pin;
    uint8_t num_spots;
} gpio_floor_config_t;

static const gpio_floor_config_t GPIO_CONFIGS[MAX_FLOORS] = {
    {
        .address_pins = {GPIO_TERREO_ENDERECO_01, GPIO_TERREO_ENDERECO_02, 0},
        .num_address_bits = 2,
        .sensor_pin = GPIO_TERREO_SENSOR_VAGA,
        .num_spots = SPOTS_TERREO
    },
    {
        .address_pins = {GPIO_ANDAR1_ENDERECO_01, GPIO_ANDAR1_ENDERECO_02, GPIO_ANDAR1_ENDERECO_03},
        .num_address_bits = 3,
        .sensor_pin = GPIO_ANDAR1_SENSOR_VAGA,
        .num_spots = SPOTS_ANDAR1
    },
    {
        .address_pins = {GPIO_ANDAR2_ENDERECO_01, GPIO_ANDAR2_ENDERECO_02, GPIO_ANDAR2_ENDERECO_03},
        .num_address_bits = 3,
        .sensor_pin = GPIO_ANDAR2_SENSOR_VAGA,
        .num_spots = SPOTS_ANDAR2
    }
};

#endif