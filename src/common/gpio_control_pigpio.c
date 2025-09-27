/**
 * @file gpio_control.c
 * @brief Implementação do controle de GPIO para Raspberry Pi usando pigpio
 * 
 * Este módulo implementa as funções básicas para:
 * - Inicialização e cleanup do sistema GPIO
 * - Configuração de pinos como entrada/saída
 * - Leitura de sensores
 * - Controle de motores/atuadores
 * - Multiplexação para endereçamento de vagas
 */

#include "gpio_control.h"
#include "system_logger.h"
#include <pigpio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

// =============================================================================
// DEFINIÇÕES DO SISTEMA GPIO
// =============================================================================

// Estado de inicialização
static bool gpio_initialized = false;

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

/**
 * @brief Inicializa o sistema GPIO usando pigpio
 * @return 0 se sucesso, -1 se erro
 */
int gpio_init(void) {
    if (gpio_initialized) {
        LOG_WARN("GPIO", "GPIO já inicializado");
        return 0;
    }
    
    // Inicializar pigpio
    int result = gpioInitialise();
    if (result < 0) {
        LOG_ERROR("GPIO", "Falha ao inicializar pigpio: %d", result);
        return -1;
    }
    
    LOG_INFO("GPIO", "pigpio inicializado - versão: %d", result);
    
    // Configurar pinos de endereçamento para cada andar
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        const gpio_floor_config_t* config = &GPIO_CONFIGS[floor];
        
        // Configurar pinos de endereçamento como saída
        for (int i = 0; i < 3; i++) {
            gpioSetMode(config->address_pins[i], PI_OUTPUT);
            gpioWrite(config->address_pins[i], 0); // Iniciar em LOW
        }
        
        // Configurar sensor de vaga como entrada com pull-up
        gpioSetMode(config->sensor_pin, PI_INPUT);
        gpioSetPullUpDown(config->sensor_pin, PI_PUD_UP);
    }
    
    // Configurar pinos específicos do térreo
    gpioSetMode(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA, PI_PUD_UP);
    
    gpioSetMode(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA, PI_PUD_UP);
    
    gpioSetMode(GPIO_TERREO_MOTOR_ENTRADA, PI_OUTPUT);
    gpioWrite(GPIO_TERREO_MOTOR_ENTRADA, 0); // Motor desligado
    
    gpioSetMode(GPIO_TERREO_SENSOR_ABERTURA_SAIDA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_ABERTURA_SAIDA, PI_PUD_UP);
    
    gpioSetMode(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA, PI_PUD_UP);
    
    gpioSetMode(GPIO_TERREO_MOTOR_SAIDA, PI_OUTPUT);
    gpioWrite(GPIO_TERREO_MOTOR_SAIDA, 0); // Motor desligado
    
    // Configurar pinos de passagem dos andares superiores
    gpioSetMode(GPIO_ANDAR1_SENSOR_PASSAGEM_1, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR1_SENSOR_PASSAGEM_1, PI_PUD_UP);
    
    gpioSetMode(GPIO_ANDAR1_SENSOR_PASSAGEM_2, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR1_SENSOR_PASSAGEM_2, PI_PUD_UP);
    
    gpioSetMode(GPIO_ANDAR2_SENSOR_PASSAGEM_1, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR2_SENSOR_PASSAGEM_1, PI_PUD_UP);
    
    gpioSetMode(GPIO_ANDAR2_SENSOR_PASSAGEM_2, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR2_SENSOR_PASSAGEM_2, PI_PUD_UP);
    
    gpio_initialized = true;
    LOG_INFO("GPIO", "GPIO inicializado com sucesso usando pigpio");
    return 0;
}

/**
 * @brief Finaliza o sistema GPIO
 */
void gpio_cleanup(void) {
    if (!gpio_initialized) return;
    
    LOG_INFO("GPIO", "Fazendo cleanup do sistema GPIO...");
    
    // Desligar todos os motores
    gpioWrite(GPIO_TERREO_MOTOR_ENTRADA, 0);
    gpioWrite(GPIO_TERREO_MOTOR_SAIDA, 0);
    
    // Zerar todos os pinos de endereçamento
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        const gpio_floor_config_t* config = &GPIO_CONFIGS[floor];
        for (int i = 0; i < 3; i++) {
            gpioWrite(config->address_pins[i], 0);
        }
    }
    
    // Finalizar pigpio
    gpioTerminate();
    gpio_initialized = false;
    LOG_INFO("GPIO", "GPIO finalizado");
}

/**
 * @brief Define o endereço no multiplexador para um andar específico
 * @param floor_id ID do andar (0=térreo, 1=1º andar, 2=2º andar)
 * @param address Endereço da vaga (0-7)
 */
void gpio_set_address(uint8_t floor_id, uint8_t address) {
    if (!gpio_initialized || floor_id >= MAX_FLOORS || address > 7) {
        LOG_ERROR("GPIO", "Parâmetros inválidos: floor=%d, addr=%d", floor_id, address);
        return;
    }
    
    const gpio_floor_config_t* config = &GPIO_CONFIGS[floor_id];
    
    // Configurar os 3 bits do endereço (A0, A1, A2)
    gpioWrite(config->address_pins[0], (address & 0x01) ? 1 : 0); // A0
    gpioWrite(config->address_pins[1], (address & 0x02) ? 1 : 0); // A1  
    gpioWrite(config->address_pins[2], (address & 0x04) ? 1 : 0); // A2
    
    // Aguardar estabilização do multiplexador (mínimo 100ns, usamos 1µs)
    gpioDelay(1);
}

/**
 * @brief Lê o estado de um sensor de vaga
 * @param floor_id ID do andar
 * @param spot_id ID da vaga (0-7)
 * @return true se ocupada, false se livre
 */
bool gpio_read_parking_sensor(uint8_t floor_id, uint8_t spot_id) {
    if (!gpio_initialized || floor_id >= MAX_FLOORS || spot_id > 7) {
        LOG_ERROR("GPIO", "Parâmetros inválidos: floor=%d, spot=%d", floor_id, spot_id);
        return false;
    }
    
    const gpio_floor_config_t* config = &GPIO_CONFIGS[floor_id];
    
    // Configurar o endereço no multiplexador
    gpio_set_address(floor_id, spot_id);
    
    // Ler o sensor (invertido: LOW = ocupado, HIGH = livre)
    int sensor_value = gpioRead(config->sensor_pin);
    bool occupied = (sensor_value == 0);
    
    LOG_DEBUG("GPIO", "Floor %d, Spot %d: %s (raw=%d)", 
              floor_id, spot_id, occupied ? "OCUPADA" : "LIVRE", sensor_value);
    
    return occupied;
}

/**
 * @brief Lê o estado de um sensor de gate
 * @param pin Pino GPIO do sensor
 * @return true se ativado, false se não ativado
 */
bool gpio_read_gate_sensor(uint8_t pin) {
    if (!gpio_initialized) return false;
    
    // Sensores são normalmente abertos (LOW quando ativados)
    int value = gpioRead(pin);
    return (value == 0);
}

/**
 * @brief Controla o motor de uma cancela
 * @param pin Pino GPIO do motor
 * @param state true para abrir, false para parar/fechar
 */
void gpio_set_gate_motor(uint8_t pin, bool state) {
    if (!gpio_initialized) return;
    
    gpioWrite(pin, state ? 1 : 0);
    LOG_DEBUG("GPIO", "Motor pin %d: %s", pin, state ? "LIGADO" : "DESLIGADO");
}

/**
 * @brief Verifica se o GPIO está inicializado
 * @return true se inicializado
 */
bool gpio_is_initialized(void) {
    return gpio_initialized;
}

/**
 * @brief Função de teste para verificar todos os sensores
 */
void gpio_test_all_sensors(void) {
    if (!gpio_initialized) {
        LOG_ERROR("GPIO", "GPIO não inicializado");
        return;
    }
    
    LOG_INFO("GPIO", "=== TESTE DE TODOS OS SENSORES ===");
    
    // Testar sensores de vagas
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        LOG_INFO("GPIO", "--- Andar %d ---", floor);
        for (int spot = 0; spot < 8; spot++) {
            bool occupied = gpio_read_parking_sensor(floor, spot);
            LOG_INFO("GPIO", "Vaga %d: %s", spot, occupied ? "OCUPADA" : "LIVRE");
            usleep(50000); // 50ms entre leituras
        }
    }
    
    // Testar sensores de gate do térreo
    LOG_INFO("GPIO", "--- Sensores de Gate ---");
    bool sensor_abertura_entrada = gpio_read_gate_sensor(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA);
    bool sensor_fechamento_entrada = gpio_read_gate_sensor(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA);
    bool sensor_abertura_saida = gpio_read_gate_sensor(GPIO_TERREO_SENSOR_ABERTURA_SAIDA);
    bool sensor_fechamento_saida = gpio_read_gate_sensor(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA);
    
    LOG_INFO("GPIO", "Entrada - Abertura: %s, Fechamento: %s", 
             sensor_abertura_entrada ? "ATIVO" : "INATIVO",
             sensor_fechamento_entrada ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "Saída - Abertura: %s, Fechamento: %s", 
             sensor_abertura_saida ? "ATIVO" : "INATIVO",
             sensor_fechamento_saida ? "ATIVO" : "INATIVO");
    
    // Testar sensores de passagem
    bool passagem_1_andar1 = gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_1);
    bool passagem_2_andar1 = gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_2);
    bool passagem_1_andar2 = gpio_read_gate_sensor(GPIO_ANDAR2_SENSOR_PASSAGEM_1);
    bool passagem_2_andar2 = gpio_read_gate_sensor(GPIO_ANDAR2_SENSOR_PASSAGEM_2);
    
    LOG_INFO("GPIO", "Passagem 1º andar: S1=%s, S2=%s", 
             passagem_1_andar1 ? "ATIVO" : "INATIVO",
             passagem_2_andar1 ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "Passagem 2º andar: S1=%s, S2=%s", 
             passagem_1_andar2 ? "ATIVO" : "INATIVO",
             passagem_2_andar2 ? "ATIVO" : "INATIVO");
}