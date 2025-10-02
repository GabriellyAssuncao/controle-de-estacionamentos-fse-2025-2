/**
 * @file  
 * @brief Cliente MODBUS RTU para comunicação com dispositivos do estacionamento
 */

#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

 
// CONFIGURAÇÕES MODBUS 

#define MODBUS_DEFAULT_DEVICE "/dev/ttyUSB0"
#define MODBUS_DEFAULT_BAUDRATE 115200
#define MODBUS_TIMEOUT_SEC 0
#define MODBUS_TIMEOUT_USEC 500000  // 500ms
#define MODBUS_MAX_RETRIES 3
#define MODBUS_RETRY_DELAY_MS 100

// Endereços dos dispositivos
#define MODBUS_ADDR_CAMERA_ENTRADA 0x11
#define MODBUS_ADDR_CAMERA_SAIDA   0x12
#define MODBUS_ADDR_DISPLAY        0x20

// Registradores das câmeras LPR
#define LPR_REG_STATUS      0
#define LPR_REG_TRIGGER     1
#define LPR_REG_PLATE       2   
#define LPR_REG_CONFIDENCE  6
#define LPR_REG_ERROR       7

// Valores de status da câmera
#define LPR_STATUS_READY       0
#define LPR_STATUS_PROCESSING  1
#define LPR_STATUS_OK          2
#define LPR_STATUS_ERROR       3

// Registradores do display
#define DISPLAY_REG_TERREO_PNE      0
#define DISPLAY_REG_TERREO_IDOSO    1
#define DISPLAY_REG_TERREO_COMUM    2
#define DISPLAY_REG_ANDAR1_PNE      3
#define DISPLAY_REG_ANDAR1_IDOSO    4
#define DISPLAY_REG_ANDAR1_COMUM    5
#define DISPLAY_REG_ANDAR2_PNE      6
#define DISPLAY_REG_ANDAR2_IDOSO    7
#define DISPLAY_REG_ANDAR2_COMUM    8
#define DISPLAY_REG_TOTAL_PNE       9
#define DISPLAY_REG_TOTAL_IDOSO     10
#define DISPLAY_REG_TOTAL_COMUM     11
#define DISPLAY_REG_FLAGS           12

 
#define DISPLAY_FLAG_LOTADO_GERAL   (1 << 0)
#define DISPLAY_FLAG_BLOQ_ANDAR1    (1 << 1)
#define DISPLAY_FLAG_BLOQ_ANDAR2    (1 << 2)
 
#define MIN_PLATE_CONFIDENCE 70

 
// TIPOS DE DADOS
 

/**
 * @brief Tipo de câmera LPR
 */
typedef enum {
    CAMERA_ENTRADA = MODBUS_ADDR_CAMERA_ENTRADA,
    CAMERA_SAIDA   = MODBUS_ADDR_CAMERA_SAIDA
} camera_type_t;

/**
 * @brief Resultado da leitura de placa
 */
typedef struct {
    char plate[9];          // Placa (8 caracteres + \0)
    int confidence;         // Confiança (0-100)
    bool success;           // Se a leitura foi bem-sucedida
    time_t timestamp;       // Timestamp da leitura
} plate_reading_t;

/**
 * @brief Informações do display
 */
typedef struct {
    // Vagas por andar e tipo
    uint8_t terreo_pne;
    uint8_t terreo_idoso;
    uint8_t terreo_comum;
    
    uint8_t andar1_pne;
    uint8_t andar1_idoso;
    uint8_t andar1_comum;
    
    uint8_t andar2_pne;
    uint8_t andar2_idoso;
    uint8_t andar2_comum;
    
    // Totais
    uint8_t total_pne;
    uint8_t total_idoso;
    uint8_t total_comum;
    
    // Flags de status
    bool lotado_geral;
    bool bloqueado_andar1;
    bool bloqueado_andar2;
} display_info_t;

/**
 * @brief Estatísticas do cliente MODBUS
 */
typedef struct {
    uint32_t requests_sent;
    uint32_t responses_received;
    uint32_t errors;
    uint32_t timeouts;
    uint32_t crc_errors;
} modbus_stats_t;

 
// FUNÇÕES PÚBLICAS
 
/**
 * @brief  
 * @param device 
 * @param baudrate  
 * @return  0 se sucesso, -1 se erro
 */
int modbus_init(const char* device, int baudrate);

/**
 * @brief Finaliza o cliente MODBUS
 */
void modbus_cleanup(void);

/**
 * @brief  
 * @return  
 */
bool modbus_is_initialized(void);

 
// FUNÇÕES DE CÂMERA LPR
 

/**
 * @brief 
 * @param camera  
 * @return  
 */
int modbus_camera_trigger(camera_type_t camera);

/**
 * @brief  
 * @param camera  
 * @param result 
 * @param timeout_ms  
 * @return 0 se sucesso, -1 se erro
 */
int modbus_camera_read_plate(camera_type_t camera, plate_reading_t* result, int timeout_ms);

/**
 * @brief  o
 * @param camera  
 * @param result  
 * @return 0 se sucesso, -1 se erro
 */
int modbus_camera_capture_and_read(camera_type_t camera, plate_reading_t* result);

/**
 * @brief  
 * @param camera  
 * @return Status (0-3) ou -1 se erro
 */
int modbus_camera_get_status(camera_type_t camera);

/**
 * @brief  
 * @param camera 
 * @return 0 se sucesso, -1 se erro
 */
int modbus_camera_reset(camera_type_t camera);

 
// FUNÇÕES DE DISPLAY 
/**
 * @brief  
 * @param info  
 * @return 0 se sucesso, -1 se erro
 */
int modbus_display_update(const display_info_t* info);

/**
 * @brief  
 * @param floor Andar (0=térreo, 1=andar1, 2=andar2)
 * @param pne Vagas PNE
 * @param idoso Vagas Idoso+
 * @param comum Vagas comuns
 * @return 0 se sucesso, -1 se erro
 */
int modbus_display_update_floor(int floor, uint8_t pne, uint8_t idoso, uint8_t comum);

/**
 * @brief Atualiza flags de status do display
 * @param lotado Sistema lotado
 * @param bloq_andar1 Andar 1 bloqueado
 * @param bloq_andar2 Andar 2 bloqueado
 * @return 0 se sucesso, -1 se erro
 */
int modbus_display_update_flags(bool lotado, bool bloq_andar1, bool bloq_andar2);

/**
 * @brief Lê informações atuais do display
 * @param info Estrutura para armazenar informações lidas
 * @return 0 se sucesso, -1 se erro
 */
int modbus_display_read(display_info_t* info);
 
// FUNÇÕES DE DIAGNÓSTICO  

/**
 * @brief  
 * @return  
 */
int modbus_test_all_devices(void);

/**
 * @brief  
 * @param address  
 * @return  
 */
int modbus_test_device(uint8_t address);

/**
 * @brief Obtém estatísticas do cliente MODBUS
 * @param stats Estrutura para armazenar estatísticas
 */
void modbus_get_stats(modbus_stats_t* stats);

/**
 * @brief Reseta estatísticas
 */
void modbus_reset_stats(void);

/**
 * @brief  
 */
void modbus_print_diagnostics(void);
 
// FUNÇÕES DE CONFIGURAÇÃO  

/**
 * @brief  
 * @param timeout_ms  
 */
void modbus_set_timeout(int timeout_ms);

/**
 * @brief  
 * @param retries 
 */
void modbus_set_retries(int retries);

/**
 * @brief 
 * @param enable true para habilitar
 */
void modbus_set_debug(bool enable);

#endif  