/**
 * @file gate_control.c
 * @brief Sistema de controle das cancelas de entrada e saída
 */

#include "gate_control.h"
#include "system_logger.h"
#include "gpio_control.h"
#include <pthread.h>
#include <unistd.h>

// =============================================================================
// ESTRUTURAS INTERNAS
// =============================================================================

typedef struct {
    gate_state_t state;
    uint8_t motor_pin;
    uint8_t sensor_open_pin;
    uint8_t sensor_close_pin;
    pthread_mutex_t mutex;
    pthread_t control_thread;
    bool thread_running;
    time_t last_operation;
    uint32_t operation_count;
    gate_type_t gate_type;
} gate_control_t;

// =============================================================================
// VARIÁVEIS GLOBAIS
// =============================================================================

static gate_control_t entry_gate = {0};
static gate_control_t exit_gate = {0};
static bool gates_initialized = false;

// =============================================================================
// FUNÇÕES INTERNAS
// =============================================================================

/**
 * @brief Thread de controle de uma cancela específica
 */
static void* gate_control_thread(void* arg) {
    gate_control_t* gate = (gate_control_t*)arg;
    const char* gate_name = (gate->gate_type == GATE_ENTRY) ? "ENTRADA" : "SAÍDA";
    
    LOG_INFO("GATE", "Thread de controle da cancela %s iniciada", gate_name);
    
    while (gate->thread_running) {
        pthread_mutex_lock(&gate->mutex);
        
        bool sensor_open = gpio_read_gate_sensor(gate->sensor_open_pin);
        bool sensor_close = gpio_read_gate_sensor(gate->sensor_close_pin);
        
        switch (gate->state) {
            case GATE_STATE_CLOSED:
                // Cancela fechada - motor desligado
                gpio_set_gate_motor(gate->motor_pin, false);
                break;
                
            case GATE_STATE_OPENING:
                // Abrindo - motor ligado, verifica sensor de abertura
                gpio_set_gate_motor(gate->motor_pin, true);
                
                if (sensor_open) {
                    gate->state = GATE_STATE_OPEN;
                    gate->last_operation = time(NULL);
                    gate->operation_count++;
                    LOG_INFO("GATE", "Cancela %s ABERTA (operação #%d)", 
                             gate_name, gate->operation_count);
                }
                
                // Timeout de segurança
                if ((time(NULL) - gate->last_operation) > (GATE_TIMEOUT_MS / 1000)) {
                    gate->state = GATE_STATE_ERROR;
                    LOG_ERROR("GATE", "Timeout ao abrir cancela %s", gate_name);
                }
                break;
                
            case GATE_STATE_OPEN:
                // Aberta - motor desligado
                gpio_set_gate_motor(gate->motor_pin, false);
                break;
                
            case GATE_STATE_CLOSING:
                // Fechando - motor ligado (direção reversa se aplicável)
                gpio_set_gate_motor(gate->motor_pin, true);
                
                if (sensor_close) {
                    gate->state = GATE_STATE_CLOSED;
                    gate->last_operation = time(NULL);
                    gate->operation_count++;
                    LOG_INFO("GATE", "Cancela %s FECHADA (operação #%d)", 
                             gate_name, gate->operation_count);
                }
                
                // Timeout de segurança
                if ((time(NULL) - gate->last_operation) > (GATE_TIMEOUT_MS / 1000)) {
                    gate->state = GATE_STATE_ERROR;
                    LOG_ERROR("GATE", "Timeout ao fechar cancela %s", gate_name);
                }
                break;
                
            case GATE_STATE_ERROR:
                // Erro - motor desligado, aguarda reset
                gpio_set_gate_motor(gate->motor_pin, false);
                LOG_DEBUG("GATE", "Cancela %s em estado de erro", gate_name);
                break;
        }
        
        pthread_mutex_unlock(&gate->mutex);
        
        // Sleep de 100ms
        usleep(100000);
    }
    
    LOG_INFO("GATE", "Thread de controle da cancela %s finalizada", gate_name);
    return NULL;
}

/**
 * @brief Inicializa uma cancela específica
 */
static int init_gate(gate_control_t* gate, gate_type_t type) {
    gate->gate_type = type;
    gate->state = GATE_STATE_CLOSED;
    gate->last_operation = time(NULL);
    gate->operation_count = 0;
    gate->thread_running = true;
    
    if (pthread_mutex_init(&gate->mutex, NULL) != 0) {
        LOG_ERROR("GATE", "Erro ao inicializar mutex da cancela %d", type);
        return -1;
    }
    
    // Configura pinos conforme o tipo
    if (type == GATE_ENTRY) {
        gate->motor_pin = GPIO_TERREO_MOTOR_ENTRADA;
        gate->sensor_open_pin = GPIO_TERREO_SENSOR_ABERTURA_ENTRADA;
        gate->sensor_close_pin = GPIO_TERREO_SENSOR_FECHAMENTO_ENTRADA;
    } else {
        gate->motor_pin = GPIO_TERREO_MOTOR_SAIDA;
        gate->sensor_open_pin = GPIO_TERREO_SENSOR_ABERTURA_SAIDA;
        gate->sensor_close_pin = GPIO_TERREO_SENSOR_FECHAMENTO_SAIDA;
    }
    
    // Inicia thread de controle
    if (pthread_create(&gate->control_thread, NULL, gate_control_thread, gate) != 0) {
        LOG_ERROR("GATE", "Erro ao criar thread da cancela %d", type);
        pthread_mutex_destroy(&gate->mutex);
        return -1;
    }
    
    return 0;
}

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

int gate_system_init(void) {
    if (gates_initialized) {
        LOG_WARN("GATE", "Sistema de cancelas já inicializado");
        return 0;
    }
    
    LOG_INFO("GATE", "Inicializando sistema de controle de cancelas...");
    
    // Inicializa cancela de entrada
    if (init_gate(&entry_gate, GATE_ENTRY) != 0) {
        LOG_ERROR("GATE", "Erro ao inicializar cancela de entrada");
        return -1;
    }
    
    // Inicializa cancela de saída
    if (init_gate(&exit_gate, GATE_EXIT) != 0) {
        LOG_ERROR("GATE", "Erro ao inicializar cancela de saída");
        gate_system_cleanup();
        return -1;
    }
    
    gates_initialized = true;
    LOG_INFO("GATE", "Sistema de cancelas inicializado com sucesso");
    
    return 0;
}

void gate_system_cleanup(void) {
    if (!gates_initialized) return;
    
    LOG_INFO("GATE", "Finalizando sistema de cancelas...");
    
    // Para threads
    entry_gate.thread_running = false;
    exit_gate.thread_running = false;
    
    // Aguarda threads terminarem
    if (entry_gate.control_thread) {
        pthread_join(entry_gate.control_thread, NULL);
    }
    
    if (exit_gate.control_thread) {
        pthread_join(exit_gate.control_thread, NULL);
    }
    
    // Desliga motores
    gpio_set_gate_motor(entry_gate.motor_pin, false);
    gpio_set_gate_motor(exit_gate.motor_pin, false);
    
    // Destroi mutexes
    pthread_mutex_destroy(&entry_gate.mutex);
    pthread_mutex_destroy(&exit_gate.mutex);
    
    gates_initialized = false;
    LOG_INFO("GATE", "Sistema de cancelas finalizado");
}

int gate_open(gate_type_t gate_type) {
    if (!gates_initialized) {
        LOG_ERROR("GATE", "Sistema de cancelas não inicializado");
        return -1;
    }
    
    gate_control_t* gate = (gate_type == GATE_ENTRY) ? &entry_gate : &exit_gate;
    const char* gate_name = (gate_type == GATE_ENTRY) ? "ENTRADA" : "SAÍDA";
    
    pthread_mutex_lock(&gate->mutex);
    
    if (gate->state == GATE_STATE_ERROR) {
        LOG_ERROR("GATE", "Cancela %s em estado de erro - não é possível abrir", gate_name);
        pthread_mutex_unlock(&gate->mutex);
        return -1;
    }
    
    if (gate->state == GATE_STATE_OPEN || gate->state == GATE_STATE_OPENING) {
        LOG_INFO("GATE", "Cancela %s já está aberta ou abrindo", gate_name);
        pthread_mutex_unlock(&gate->mutex);
        return 0;
    }
    
    LOG_INFO("GATE", "Comando para abrir cancela %s", gate_name);
    gate->state = GATE_STATE_OPENING;
    gate->last_operation = time(NULL);
    
    pthread_mutex_unlock(&gate->mutex);
    
    return 0;
}

int gate_close(gate_type_t gate_type) {
    if (!gates_initialized) {
        LOG_ERROR("GATE", "Sistema de cancelas não inicializado");
        return -1;
    }
    
    gate_control_t* gate = (gate_type == GATE_ENTRY) ? &entry_gate : &exit_gate;
    const char* gate_name = (gate_type == GATE_ENTRY) ? "ENTRADA" : "SAÍDA";
    
    pthread_mutex_lock(&gate->mutex);
    
    if (gate->state == GATE_STATE_ERROR) {
        LOG_ERROR("GATE", "Cancela %s em estado de erro - não é possível fechar", gate_name);
        pthread_mutex_unlock(&gate->mutex);
        return -1;
    }
    
    if (gate->state == GATE_STATE_CLOSED || gate->state == GATE_STATE_CLOSING) {
        LOG_INFO("GATE", "Cancela %s já está fechada ou fechando", gate_name);
        pthread_mutex_unlock(&gate->mutex);
        return 0;
    }
    
    LOG_INFO("GATE", "Comando para fechar cancela %s", gate_name);
    gate->state = GATE_STATE_CLOSING;
    gate->last_operation = time(NULL);
    
    pthread_mutex_unlock(&gate->mutex);
    
    return 0;
}

gate_state_t gate_get_state(gate_type_t gate_type) {
    if (!gates_initialized) {
        return GATE_STATE_ERROR;
    }
    
    gate_control_t* gate = (gate_type == GATE_ENTRY) ? &entry_gate : &exit_gate;
    
    pthread_mutex_lock(&gate->mutex);
    gate_state_t state = gate->state;
    pthread_mutex_unlock(&gate->mutex);
    
    return state;
}

int gate_reset_error(gate_type_t gate_type) {
    if (!gates_initialized) {
        LOG_ERROR("GATE", "Sistema de cancelas não inicializado");
        return -1;
    }
    
    gate_control_t* gate = (gate_type == GATE_ENTRY) ? &entry_gate : &exit_gate;
    const char* gate_name = (gate_type == GATE_ENTRY) ? "ENTRADA" : "SAÍDA";
    
    pthread_mutex_lock(&gate->mutex);
    
    if (gate->state == GATE_STATE_ERROR) {
        LOG_INFO("GATE", "Resetando erro da cancela %s", gate_name);
        
        // Determina estado baseado nos sensores
        bool sensor_open = gpio_read_gate_sensor(gate->sensor_open_pin);
        bool sensor_close = gpio_read_gate_sensor(gate->sensor_close_pin);
        
        if (sensor_close) {
            gate->state = GATE_STATE_CLOSED;
        } else if (sensor_open) {
            gate->state = GATE_STATE_OPEN;
        } else {
            // Estado indeterminado, assume fechada
            gate->state = GATE_STATE_CLOSED;
        }
        
        gate->last_operation = time(NULL);
        LOG_INFO("GATE", "Cancela %s resetada para estado: %d", gate_name, gate->state);
    }
    
    pthread_mutex_unlock(&gate->mutex);
    
    return 0;
}

void gate_emergency_open_all(void) {
    if (!gates_initialized) return;
    
    LOG_WARN("GATE", "EMERGÊNCIA - Abrindo todas as cancelas");
    
    gate_open(GATE_ENTRY);
    gate_open(GATE_EXIT);
}

void gate_print_status(void) {
    if (!gates_initialized) {
        printf("Sistema de cancelas não inicializado\n");
        return;
    }
    
    const char* state_names[] = {
        "FECHADA", "ABRINDO", "ABERTA", "FECHANDO", "ERRO"
    };
    
    printf("\n=== STATUS DAS CANCELAS ===\n");
    
    pthread_mutex_lock(&entry_gate.mutex);
    printf("ENTRADA: %s (operações: %d)\n", 
           state_names[entry_gate.state], entry_gate.operation_count);
    printf("  Sensores: Abertura=%s, Fechamento=%s\n",
           gpio_read_gate_sensor(entry_gate.sensor_open_pin) ? "ATIVO" : "INATIVO",
           gpio_read_gate_sensor(entry_gate.sensor_close_pin) ? "ATIVO" : "INATIVO");
    pthread_mutex_unlock(&entry_gate.mutex);
    
    pthread_mutex_lock(&exit_gate.mutex);
    printf("SAÍDA: %s (operações: %d)\n", 
           state_names[exit_gate.state], exit_gate.operation_count);
    printf("  Sensores: Abertura=%s, Fechamento=%s\n",
           gpio_read_gate_sensor(exit_gate.sensor_open_pin) ? "ATIVO" : "INATIVO",
           gpio_read_gate_sensor(exit_gate.sensor_close_pin) ? "ATIVO" : "INATIVO");
    pthread_mutex_unlock(&exit_gate.mutex);
    
    printf("===========================\n\n");
}