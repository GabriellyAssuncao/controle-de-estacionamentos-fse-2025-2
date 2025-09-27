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
    
    gpio_initialized = true;
    LOG_INFO("GPIO", "GPIO inicializado com sucesso");
    return 0;
}
    
    // Inicializa todos os pinos de endereçamento como saída
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        const gpio_floor_config_t* config = &GPIO_CONFIGS[floor];
        
        for (int i = 0; i < 3; i++) {
            gpio_set_function(config->address_pins[i], 1); // Output
            gpio_write_pin(config->address_pins[i], false); // Initialize LOW
        }
        
        // Configura o sensor de vaga como entrada
        gpio_set_function(config->sensor_pin, 0); // Input
    }
    
    // Configura pinos específicos do térreo
    gpio_set_function(GPIO_TERREO_SENSOR_ABERTURA_ENTRADA, 0);   // Input
    gpio_set_function(GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA, 0); // Input
    gpio_set_function(GPIO_TERREO_MOTOR_ENTRADA, 1);             // Output
    gpio_set_function(GPIO_TERREO_SENSOR_ABERTURA_SAIDA, 0);     // Input
    gpio_set_function(GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA, 0);   // Input
    gpio_set_function(GPIO_TERREO_MOTOR_SAIDA, 1);               // Output
    
    // Motores inicialmente desligados
    gpio_write_pin(GPIO_TERREO_MOTOR_ENTRADA, false);
    gpio_write_pin(GPIO_TERREO_MOTOR_SAIDA, false);
    
    // Configura pinos de passagem dos andares superiores
    gpio_set_function(GPIO_ANDAR1_SENSOR_PASSAGEM_1, 0); // Input
    gpio_set_function(GPIO_ANDAR1_SENSOR_PASSAGEM_2, 0); // Input
    gpio_set_function(GPIO_ANDAR2_SENSOR_PASSAGEM_1, 0); // Input
    gpio_set_function(GPIO_ANDAR2_SENSOR_PASSAGEM_2, 0); // Input
    
    return 0;
}

void gpio_cleanup(void) {
    if (!gpio_initialized) return;
    
    LOG_INFO("GPIO", "Fazendo cleanup do sistema GPIO...");
    
    // Desliga todos os motores
    gpio_write_pin(GPIO_TERREO_MOTOR_ENTRADA, false);
    gpio_write_pin(GPIO_TERREO_MOTOR_SAIDA, false);
    
    // Zera todos os pinos de endereçamento
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        const gpio_floor_config_t* config = &GPIO_CONFIGS[floor];
        for (int i = 0; i < 3; i++) {
            gpio_write_pin(config->address_pins[i], false);
        }
    }
    
    // Desmapeia a memória
    if (gpio_map != MAP_FAILED) {
        munmap((void*)gpio_map, BLOCK_SIZE);
    }
    
    // Fecha o file descriptor
    if (mem_fd >= 0) {
        close(mem_fd);
    }
    
    gpio_initialized = false;
    LOG_INFO("GPIO", "Cleanup do GPIO concluído");
}

int gpio_set_address(const gpio_floor_config_t* config, uint8_t address) {
    if (!gpio_initialized || !config || address > 7) {
        LOG_ERROR("GPIO", "Parâmetros inválidos para set_address");
        return -1;
    }
    
    // Configura os 3 bits de endereço (0-7)
    gpio_write_pin(config->address_pins[0], address & 0x01);
    gpio_write_pin(config->address_pins[1], address & 0x02);
    gpio_write_pin(config->address_pins[2], address & 0x04);
    
    // Aguarda um tempo para estabilizar o endereçamento
    usleep(1000); // 1ms
    
    LOG_DEBUG("GPIO", "Endereço %d configurado (pins %d,%d,%d)", 
              address, config->address_pins[0], config->address_pins[1], config->address_pins[2]);
    
    return 0;
}

bool gpio_read_parking_sensor(const gpio_floor_config_t* config) {
    if (!gpio_initialized || !config) {
        return false;
    }
    
    bool sensor_state = gpio_read_pin(config->sensor_pin);
    
    // Log apenas mudanças de estado para evitar spam
    static bool last_states[MAX_FLOORS] = {false};
    static int last_floor = -1;
    
    // Determina qual andar com base no endereço do sensor
    int current_floor = -1;
    for (int i = 0; i < MAX_FLOORS; i++) {
        if (GPIO_CONFIGS[i].sensor_pin == config->sensor_pin) {
            current_floor = i;
            break;
        }
    }
    
    if (current_floor >= 0 && (current_floor != last_floor || sensor_state != last_states[current_floor])) {
        LOG_DEBUG("GPIO", "Sensor andar %d (pin %d): %s", 
                  current_floor, config->sensor_pin, sensor_state ? "OCUPADO" : "LIVRE");
        last_states[current_floor] = sensor_state;
        last_floor = current_floor;
    }
    
    return sensor_state;
}

bool gpio_read_gate_sensor(uint8_t pin) {
    if (!gpio_initialized) {
        return false;
    }
    
    return gpio_read_pin(pin);
}

void gpio_set_gate_motor(uint8_t pin, bool activate) {
    if (!gpio_initialized) {
        return;
    }
    
    gpio_write_pin(pin, activate);
    
    LOG_INFO("GPIO", "Motor cancela (pin %d): %s", pin, activate ? "ATIVO" : "INATIVO");
}

// =============================================================================
// FUNÇÕES DE TESTE E DEBUG
// =============================================================================

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
            LOG_INFO("GPIO", "  Endereço %d: sensor %s", addr, sensor ? "ATIVO" : "INATIVO");
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