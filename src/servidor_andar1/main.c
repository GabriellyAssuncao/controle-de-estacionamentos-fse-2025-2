/**
 * @file servidor_andar1/main.c
 * @brief Servidor do 1º andar - controla vagas e detecta passagem entre andares
 */

#include "parking_system.h"
#include "system_logger.h"
#include "gpio_control.h"
#include "parking_logic.h"
#include "tcp_communication.h"
#include <signal.h>

// =============================================================================
// VARIÁVEIS GLOBAIS
// =============================================================================

static volatile bool running = true;
static parking_status_t g_parking_status;
static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

// Socket TCP para servidor central
static int central_socket = -1;

// Estatísticas
static struct {
    uint32_t movements_up;      // Carros subindo para andar 2
    uint32_t movements_down;    // Carros descendo para térreo
    time_t start_time;
} stats = {0};

// Estado dos sensores de passagem
static struct {
    bool sensor1_last;
    bool sensor2_last;
    time_t last_trigger;
} passage_state = {false, false, 0};

// =============================================================================
// MANIPULADORES DE SINAL
// =============================================================================

static void handle_signal(int sig) {
    (void)sig;
    running = false;
    LOG_WARN("MAIN", "Sinal de término recebido");
}

// =============================================================================
// FUNÇÕES AUXILIARES
// =============================================================================

/**
 * @brief Envia status de vagas para o servidor central
 */
static void send_status_to_central(void) {
    if (central_socket < 0) return;
    
    pthread_mutex_lock(&status_mutex);
    
    system_message_t msg;
    msg.type = MSG_TYPE_PARKING_STATUS;
    msg.timestamp = time(NULL);
    
    floor_status_t *floor = &g_parking_status.floors[FLOOR_ANDAR1];
    msg.data.parking_status.andar1_pne = floor->free_pne;
    msg.data.parking_status.andar1_idoso = floor->free_idoso;
    msg.data.parking_status.andar1_comum = floor->free_comum;
    msg.data.parking_status.cars_andar1 = floor->cars_count;
    
    pthread_mutex_unlock(&status_mutex);
    
    if (tcp_send_message(central_socket, &msg) != 0) {
        LOG_WARN("TCP", "Erro ao enviar status para central");
    }
}

/**
 * @brief Detecta direção de movimento baseado na sequência de sensores
 * @return 1 = subindo (1->2), -1 = descendo (2->1), 0 = sem movimento
 */
static int detect_passage_direction(bool s1, bool s2) {
    static enum {
        STATE_IDLE,
        STATE_S1_ACTIVE,
        STATE_S2_ACTIVE,
        STATE_BOTH_ACTIVE
    } state = STATE_IDLE;
    
    static bool entered_from_s1 = false;
    int direction = 0;
    
    // Timeout de segurança (resetar se muito tempo passou)
    time_t now = time(NULL);
    if (now - passage_state.last_trigger > 5) {
        state = STATE_IDLE;
        entered_from_s1 = false;
    }
    
    switch (state) {
        case STATE_IDLE:
            if (s1 && !s2) {
                state = STATE_S1_ACTIVE;
                entered_from_s1 = true;
                passage_state.last_trigger = now;
                LOG_DEBUG("PASSAGE", "S1 ativado - possível movimento 1->2");
            } else if (!s1 && s2) {
                state = STATE_S2_ACTIVE;
                entered_from_s1 = false;
                passage_state.last_trigger = now;
                LOG_DEBUG("PASSAGE", "S2 ativado - possível movimento 2->1");
            }
            break;
            
        case STATE_S1_ACTIVE:
            if (s1 && s2) {
                state = STATE_BOTH_ACTIVE;
                LOG_DEBUG("PASSAGE", "Ambos sensores ativos");
            } else if (!s1 && !s2) {
                state = STATE_IDLE;
            }
            break;
            
        case STATE_S2_ACTIVE:
            if (s1 && s2) {
                state = STATE_BOTH_ACTIVE;
                LOG_DEBUG("PASSAGE", "Ambos sensores ativos");
            } else if (!s1 && !s2) {
                state = STATE_IDLE;
            }
            break;
            
        case STATE_BOTH_ACTIVE:
            if (!s1 && s2 && entered_from_s1) {
                // Entrou por S1, saiu por S2 = subindo (1->2)
                direction = 1;
                LOG_INFO("PASSAGE", "Movimento detectado: 1º andar -> 2º andar");
                state = STATE_S2_ACTIVE;
            } else if (s1 && !s2 && !entered_from_s1) {
                // Entrou por S2, saiu por S1 = descendo (2->1)
                direction = -1;
                LOG_INFO("PASSAGE", "Movimento detectado: 2º andar -> 1º andar");
                state = STATE_S1_ACTIVE;
            } else if (!s1 && !s2) {
                state = STATE_IDLE;
            }
            break;
    }
    
    return direction;
}

// =============================================================================
// THREADS DE CONTROLE
// =============================================================================

/**
 * @brief Thread de varredura de vagas
 */
static void* gpio_scan_thread(void* arg) {
    (void)arg;
    
    LOG_INFO("THREAD", "Thread de varredura de vagas iniciada");
    
    const gpio_floor_config_t* config = &GPIO_CONFIGS[FLOOR_ANDAR1];
    
    while (running) {
        pthread_mutex_lock(&status_mutex);
        
        int changes = parking_scan_floor(FLOOR_ANDAR1, config, 
                                        &g_parking_status.floors[FLOOR_ANDAR1]);
        
        if (changes > 0) {
            parking_update_total_stats(&g_parking_status);
            pthread_mutex_unlock(&status_mutex);
            
            // Notificar central sobre mudanças
            send_status_to_central();
        } else {
            pthread_mutex_unlock(&status_mutex);
        }
        
        usleep(GPIO_SCAN_INTERVAL_MS * 1000);
    }
    
    LOG_INFO("THREAD", "Thread de varredura finalizada");
    return NULL;
}

/**
 * @brief Thread de detecção de passagem entre andares
 */
static void* passage_detection_thread(void* arg) {
    (void)arg;
    
    LOG_INFO("THREAD", "Thread de detecção de passagem iniciada");
    
    while (running) {
        // Ler sensores de passagem
        bool s1 = gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_1);
        bool s2 = gpio_read_gate_sensor(GPIO_ANDAR1_SENSOR_PASSAGEM_2);
        
        // Detectar direção de movimento
        int direction = detect_passage_direction(s1, s2);
        
        if (direction != 0) {
            // Atualizar estatísticas
            if (direction > 0) {
                stats.movements_up++;
            } else {
                stats.movements_down++;
            }
            
            // Enviar notificação para central
            if (central_socket >= 0) {
                system_message_t msg;
                msg.type = MSG_TYPE_PASSAGE_DETECTED;
                msg.timestamp = time(NULL);
                msg.data.passage.from_floor = (direction > 0) ? FLOOR_ANDAR1 : FLOOR_ANDAR2;
                msg.data.passage.to_floor = (direction > 0) ? FLOOR_ANDAR2 : FLOOR_ANDAR1;
                strcpy(msg.data.passage.plate, ""); // Placa desconhecida na passagem
                
                tcp_send_message(central_socket, &msg);
            }
        }
        
        // Salvar estado anterior
        passage_state.sensor1_last = s1;
        passage_state.sensor2_last = s2;
        
        usleep(50000); // 50ms - verificação rápida para não perder eventos
    }
    
    LOG_INFO("THREAD", "Thread de detecção de passagem finalizada");
    return NULL;
}

/**
 * @brief Thread de comunicação com servidor central
 */
static void* tcp_client_thread(void* arg) {
    (void)arg;
    
    LOG_INFO("THREAD", "Thread TCP cliente iniciada");
    
    while (running) {
        // Tentar conectar ao servidor central
        if (central_socket < 0) {
            LOG_INFO("TCP", "Tentando conectar ao servidor central...");
            central_socket = tcp_client_connect(SERVER_CENTRAL_IP, SERVER_CENTRAL_PORT);
            
            if (central_socket >= 0) {
                LOG_INFO("TCP", "Conectado ao servidor central");
            } else {
                LOG_WARN("TCP", "Falha ao conectar - tentando novamente em 5s");
                sleep(5);
                continue;
            }
        }
        
        // Enviar status periódico
        send_status_to_central();
        
        // TODO: Receber mensagens do central (comandos, bloqueio de andar, etc)
        
        sleep(2);
    }
    
    LOG_INFO("THREAD", "Thread TCP cliente finalizada");
    return NULL;
}

// =============================================================================
// FUNÇÃO PRINCIPAL
// =============================================================================

int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    
    // Inicializar logger
    if (logger_init(LOG_DIR) != 0) {
        fprintf(stderr, "Falha ao iniciar logger\n");
        return 1;
    }
    logger_set_level(LOG_LEVEL_INFO);
    
    LOG_INFO("MAIN", "═══════════════════════════════════════════════════");
    LOG_INFO("MAIN", "  SERVIDOR 1º ANDAR - Sistema de Estacionamento");
    LOG_INFO("MAIN", "  Versão: %s", SYSTEM_VERSION);
    LOG_INFO("MAIN", "═══════════════════════════════════════════════════");
    
    stats.start_time = time(NULL);
    
    // Inicializar GPIO
    if (gpio_init() != 0) {
        LOG_ERROR("MAIN", "Falha ao inicializar GPIO");
#ifndef MOCK_BUILD
        return 1;
#else
        LOG_WARN("MAIN", "Continuando em modo MOCK");
#endif
    }
    
    // Inicializar lógica de estacionamento
    parking_init(&g_parking_status);
    
    // Criar threads
    pthread_t thread_gpio_scan;
    pthread_t thread_passage;
    pthread_t thread_tcp;
    
    pthread_create(&thread_gpio_scan, NULL, gpio_scan_thread, NULL);
    pthread_create(&thread_passage, NULL, passage_detection_thread, NULL);
    pthread_create(&thread_tcp, NULL, tcp_client_thread, NULL);
    
    LOG_INFO("MAIN", "Todas as threads iniciadas - sistema operacional");
    
    // Loop principal
    while (running) {
        sleep(1);
    }
    
    LOG_INFO("MAIN", "Iniciando shutdown...");
    
    // Aguardar threads finalizarem
    pthread_join(thread_gpio_scan, NULL);
    pthread_join(thread_passage, NULL);
    pthread_join(thread_tcp, NULL);
    
    // Exibir estatísticas finais
    time_t uptime = time(NULL) - stats.start_time;
    LOG_INFO("MAIN", "═══════════════════════════════════════════════════");
    LOG_INFO("MAIN", "  ESTATÍSTICAS FINAIS");
    LOG_INFO("MAIN", "  Tempo de operação: %ld segundos", uptime);
    LOG_INFO("MAIN", "  Movimentos 1º->2º: %u", stats.movements_up);
    LOG_INFO("MAIN", "  Movimentos 2º->1º: %u", stats.movements_down);
    LOG_INFO("MAIN", "═══════════════════════════════════════════════════");
    
    // Cleanup
    if (central_socket >= 0) {
        tcp_close_connection(central_socket);
    }
    
    gpio_cleanup();
    logger_cleanup();
    
    LOG_INFO("MAIN", "Servidor 1º andar finalizado");
    
    return 0;
}