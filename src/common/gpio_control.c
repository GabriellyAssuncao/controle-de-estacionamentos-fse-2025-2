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
    
    // Configurar pinos de entrada (sensores)
    for (int i = 0; i < NUM_SENSOR_PINS; i++) {
        gpioSetMode(SENSOR_PINS[i], PI_INPUT);
        gpioSetPullUpDown(SENSOR_PINS[i], PI_PUD_UP); // Pull-up interno
    }
    
    // Configurar pinos de multiplexação (saída)
    for (int i = 0; i < NUM_MUX_PINS; i++) {
        gpioSetMode(MUX_PINS[i], PI_OUTPUT);
        gpioWrite(MUX_PINS[i], 0); // Iniciar em LOW
    }
    
    // Configurar pinos das cancelas (saída)
    for (int i = 0; i < NUM_GATE_PINS; i++) {
        gpioSetMode(GATE_PINS[i], PI_OUTPUT);
        gpioWrite(GATE_PINS[i], 0); // Cancelas fechadas (LOW)
    }
    
    // Configurar sensores específicos
    // Sensores de cancela do térreo
    gpioSetMode(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA, PI_PUD_UP);
    
    gpioSetMode(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA, PI_PUD_UP);
    
    gpioSetMode(GPIO_TERREO_SENSOR_ABERTURA_SAIDA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_ABERTURA_SAIDA, PI_PUD_UP);
    
    gpioSetMode(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA, PI_INPUT);
    gpioSetPullUpDown(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA, PI_PUD_UP);
    
    // Sensores de passagem dos andares
    gpioSetMode(GPIO_ANDAR1_SENSOR_PASSAGEM_1, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR1_SENSOR_PASSAGEM_1, PI_PUD_UP);
    
    gpioSetMode(GPIO_ANDAR1_SENSOR_PASSAGEM_2, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR1_SENSOR_PASSAGEM_2, PI_PUD_UP);
    
    gpioSetMode(GPIO_ANDAR2_SENSOR_PASSAGEM_1, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR2_SENSOR_PASSAGEM_1, PI_PUD_UP);
    
    gpioSetMode(GPIO_ANDAR2_SENSOR_PASSAGEM_2, PI_INPUT);
    gpioSetPullUpDown(GPIO_ANDAR2_SENSOR_PASSAGEM_2, PI_PUD_UP);
    
    // Configurar motores das cancelas
    gpioSetMode(GPIO_TERREO_MOTOR_ENTRADA, PI_OUTPUT);
    gpioWrite(GPIO_TERREO_MOTOR_ENTRADA, 0);
    
    gpioSetMode(GPIO_TERREO_MOTOR_SAIDA, PI_OUTPUT);
    gpioWrite(GPIO_TERREO_MOTOR_SAIDA, 0);
    
    gpio_initialized = true;
    LOG_INFO("GPIO", "GPIO inicializado com sucesso");
    return 0;
}

/**
 * @brief Limpa e finaliza o sistema GPIO
 */
void gpio_cleanup(void) {
    if (!gpio_initialized) return;
    
    LOG_INFO("GPIO", "Fazendo cleanup do sistema GPIO...");
    
    // Desliga todos os motores
    gpioWrite(GPIO_TERREO_MOTOR_ENTRADA, 0);
    gpioWrite(GPIO_TERREO_MOTOR_SAIDA, 0);
    
    // Zera todos os pinos de multiplexação
    for (int i = 0; i < NUM_MUX_PINS; i++) {
        gpioWrite(MUX_PINS[i], 0);
    }
    
    // Finaliza pigpio
    gpioTerminate();
    
    gpio_initialized = false;
    LOG_INFO("GPIO", "Cleanup do GPIO concluído");
}

/**
 * @brief Configura o endereço do multiplexador para um andar
 * @param config Configuração do andar
 * @param address Endereço (0-7)
 * @return 0 se sucesso, -1 se erro
 */
int gpio_set_address(const gpio_floor_config_t* config, uint8_t address) {
    if (!gpio_initialized || !config || address > 7) {
        LOG_ERROR("GPIO", "Parâmetros inválidos para set_address");
        return -1;
    }
    
    // Configura os 3 bits de endereço (0-7)
    gpioWrite(config->address_pins[0], address & 0x01);
    gpioWrite(config->address_pins[1], (address & 0x02) >> 1);
    gpioWrite(config->address_pins[2], (address & 0x04) >> 2);
    
    // Aguarda um tempo para estabilizar o endereçamento
    usleep(1000); // 1ms
    
    return 0;
}

/**
 * @brief Lê um sensor de estacionamento multiplexado
 * @param config Configuração do andar
 * @return true se vaga ocupada, false se livre
 */
bool gpio_read_parking_sensor(const gpio_floor_config_t* config) {
    if (!gpio_initialized || !config) {
        LOG_ERROR("GPIO", "Parâmetros inválidos para read_parking_sensor");
        return false;
    }
    
    // Lê o estado do sensor (invertido - LOW = ocupado)
    int value = gpioRead(config->sensor_pin);
    return (value == 0); // LOW = ocupado (sensor ativo)
}

/**
 * @brief Lê um sensor de cancela
 * @param pin Pino do sensor
 * @return true se sensor ativo, false se inativo
 */
bool gpio_read_gate_sensor(int pin) {
    if (!gpio_initialized) {
        LOG_ERROR("GPIO", "GPIO não inicializado");
        return false;
    }
    
    // Lê o estado do sensor (invertido - LOW = ativo)
    int value = gpioRead(pin);
    return (value == 0);
}

/**
 * @brief Escreve um valor em um pino digital
 * @param pin Número do pino
 * @param value true para HIGH, false para LOW
 */
void gpio_write_pin(int pin, bool value) {
    if (!gpio_initialized) {
        LOG_ERROR("GPIO", "GPIO não inicializado");
        return;
    }
    
    gpioWrite(pin, value ? 1 : 0);
}

/**
 * @brief Lê o valor de um pino digital
 * @param pin Número do pino
 * @return true se HIGH, false se LOW
 */
bool gpio_read_pin(int pin) {
    if (!gpio_initialized) {
        LOG_ERROR("GPIO", "GPIO não inicializado");
        return false;
    }
    
    return gpioRead(pin) == 1;
}

/**
 * @brief Controla um motor de cancela
 * @param pin Pino do motor
 * @param activate true para ativar, false para desativar
 */
void gpio_control_gate_motor(int pin, bool activate) {
    if (!gpio_initialized) {
        LOG_ERROR("GPIO", "GPIO não inicializado");
        return;
    }
    
    gpioWrite(pin, activate ? 1 : 0);
    
    LOG_INFO("GPIO", "Motor cancela (pin %d): %s", pin, activate ? "ATIVO" : "INATIVO");
}

// =============================================================================
// FUNÇÕES DE TESTE E DEBUG
// =============================================================================

/**
 * @brief Testa todos os pinos GPIO
 */
void gpio_test_all_pins(void) {
    if (!gpio_initialized) {
        LOG_ERROR("GPIO", "GPIO não inicializado para teste");
        return;
    }
    
    LOG_INFO("GPIO", "Iniciando teste de todos os pinos...");
    
    // Teste dos pinos de endereçamento
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        const gpio_floor_config_t* config = &GPIO_CONFIGS[floor];
        LOG_INFO("GPIO", "Testando andar %d...", floor);
        
        for (uint8_t addr = 0; addr < 8; addr++) {
            gpio_set_address(config, addr);
            bool sensor = gpio_read_parking_sensor(config);
            LOG_INFO("GPIO", "  Endereço %d: sensor %s", addr, sensor ? "OCUPADO" : "LIVRE");
            usleep(100000); // 100ms entre testes
        }
    }
    
    // Teste dos sensores de cancela (apenas térreo)
    LOG_INFO("GPIO", "Testando sensores de cancela...");
    LOG_INFO("GPIO", "  Abertura entrada: %s", 
             gpio_read_gate_sensor(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA) ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "  Fechamento entrada: %s", 
             gpio_read_gate_sensor(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA) ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "  Abertura saída: %s", 
             gpio_read_gate_sensor(GPIO_TERREO_SENSOR_ABERTURA_SAIDA) ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "  Fechamento saída: %s", 
             gpio_read_gate_sensor(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA) ? "ATIVO" : "INATIVO");
    
    // Teste dos sensores de passagem
    LOG_INFO("GPIO", "Testando sensores de passagem...");
    LOG_INFO("GPIO", "  1º andar - sensor 1: %s", 
             gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_1) ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "  1º andar - sensor 2: %s", 
             gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_2) ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "  2º andar - sensor 1: %s", 
             gpio_read_gate_sensor(GPIO_ANDAR2_SENSOR_PASSAGEM_1) ? "ATIVO" : "INATIVO");
    LOG_INFO("GPIO", "  2º andar - sensor 2: %s", 
             gpio_read_gate_sensor(GPIO_ANDAR2_SENSOR_PASSAGEM_2) ? "ATIVO" : "INATIVO");
    
    LOG_INFO("GPIO", "Teste concluído");
}

/**
 * @brief Monitora continuamente todos os sensores
 * @param duration_seconds Duração em segundos (0 = infinito)
 */
void gpio_monitor_sensors(int duration_seconds) {
    if (!gpio_initialized) {
        LOG_ERROR("GPIO", "GPIO não inicializado para monitoramento");
        return;
    }
    
    LOG_INFO("GPIO", "Iniciando monitoramento de sensores...");
    LOG_INFO("GPIO", "Pressione CTRL+C para parar");
    
    time_t start_time = time(NULL);
    
    while (1) {
        // Verifica timeout se especificado
        if (duration_seconds > 0) {
            if (time(NULL) - start_time >= duration_seconds) {
                break;
            }
        }
        
        printf("\n=== STATUS DOS SENSORES ===\n");
        
        // Monitora vagas de estacionamento
        for (int floor = 0; floor < MAX_FLOORS; floor++) {
            const gpio_floor_config_t* config = &GPIO_CONFIGS[floor];
            printf("Andar %d:\n", floor);
            
            for (uint8_t addr = 0; addr < 8; addr++) {
                gpio_set_address(config, addr);
                bool occupied = gpio_read_parking_sensor(config);
                printf("  Vaga %d: %s\n", addr, occupied ? "OCUPADA" : "LIVRE");
            }
        }
        
        // Monitora sensores de cancela
        printf("Cancelas:\n");
        printf("  Entrada - Abertura: %s\n", 
               gpio_read_gate_sensor(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA) ? "ATIVO" : "INATIVO");
        printf("  Entrada - Fechamento: %s\n", 
               gpio_read_gate_sensor(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA) ? "ATIVO" : "INATIVO");
        printf("  Saída - Abertura: %s\n", 
               gpio_read_gate_sensor(GPIO_TERREO_SENSOR_ABERTURA_SAIDA) ? "ATIVO" : "INATIVO");
        printf("  Saída - Fechamento: %s\n", 
               gpio_read_gate_sensor(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA) ? "ATIVO" : "INATIVO");
        
        // Monitora sensores de passagem
        printf("Passagem:\n");
        printf("  1º Andar S1: %s\n", 
               gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_1) ? "ATIVO" : "INATIVO");
        printf("  1º Andar S2: %s\n", 
               gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_2) ? "ATIVO" : "INATIVO");
        printf("  2º Andar S1: %s\n", 
               gpio_read_gate_sensor(GPIO_ANDAR2_SENSOR_PASSAGEM_1) ? "ATIVO" : "INATIVO");
        printf("  2º Andar S2: %s\n", 
               gpio_read_gate_sensor(GPIO_ANDAR2_SENSOR_PASSAGEM_2) ? "ATIVO" : "INATIVO");
        
        sleep(1); // Atualiza a cada segundo
    }
    
    LOG_INFO("GPIO", "Monitoramento finalizado");
}