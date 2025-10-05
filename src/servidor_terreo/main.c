/**
 * @file servidor_terreo/main.c
 * @brief Servidor do andar térreo - controla cancelas, vagas e MODBUS
 */

#include "parking_system.h"
#include "system_logger.h"
#include "gpio_control.h"
#include "gate_control.h"
#include "parking_logic.h"
#include "modbus_client.h"
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
    uint32_t vehicles_entered;
    uint32_t vehicles_exited;
    uint32_t gate_operations;
    time_t start_time;
} stats = {0};

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
    
    floor_status_t *floor = &g_parking_status.floors[FLOOR_TERREO];
    msg.data.parking_status.terreo_pne = floor->free_pne;
    msg.data.parking_status.terreo_idoso = floor->free_idoso;
    msg.data.parking_status.terreo_comum = floor->free_comum;
    msg.data.parking_status.cars_terreo = floor->cars_count;
    
    pthread_mutex_unlock(&status_mutex);
    
    if (tcp_send_message(central_socket, &msg) != 0) {
        LOG_WARN("TCP", "Erro ao enviar status para central");
    }
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
    
    const gpio_floor_config_t* config = &GPIO_CONFIGS[FLOOR_TERREO];
    
    while (running) {
        pthread_mutex_lock(&status_mutex);
        
        int changes = parking_scan_floor(FLOOR_TERREO, config, 
                                        &g_parking_status.floors[FLOOR_TERREO]);
        
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
        
        // Enviar heartbeat periódico
        send_status_to_central();
        
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
    LOG_INFO("MAIN", "  SERVIDOR TÉRREO - Sistema de Estacionamento");
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
    
    // Inicializar sistema de cancelas
    if (gate_system_init() != 0) {
        LOG_ERROR("MAIN", "Falha ao inicializar sistema de cancelas");
    }
    
    // Inicializar lógica de estacionamento
    parking_init(&g_parking_status);
    
    // Criar threads
    pthread_t thread_gpio_scan;
    pthread_t thread_tcp;
    
    pthread_create(&thread_gpio_scan, NULL, gpio_scan_thread, NULL);
    pthread_create(&thread_tcp, NULL, tcp_client_thread, NULL);
    
    LOG_INFO("MAIN", "Todas as threads iniciadas - sistema operacional");
    
    // Loop principal - apenas aguarda sinal de término
    while (running) {
        sleep(1);
    }
    
    LOG_INFO("MAIN", "Iniciando shutdown...");
    
    // Aguardar threads finalizarem
    pthread_join(thread_gpio_scan, NULL);
    pthread_join(thread_tcp, NULL);
    
    // Exibir estatísticas finais
    time_t uptime = time(NULL) - stats.start_time;
    LOG_INFO("MAIN", "═══════════════════════════════════════════════════");
    LOG_INFO("MAIN", "  ESTATÍSTICAS FINAIS");
    LOG_INFO("MAIN", "  Tempo de operação: %ld segundos", uptime);
    LOG_INFO("MAIN", "  Veículos entrada: %u", stats.vehicles_entered);
    LOG_INFO("MAIN", "  Veículos saída: %u", stats.vehicles_exited);
    LOG_INFO("MAIN", "  Operações de cancela: %u", stats.gate_operations);
    LOG_INFO("MAIN", "═══════════════════════════════════════════════════");
    
    // Cleanup
    if (central_socket >= 0) {
        tcp_close_connection(central_socket);
    }
    
    gate_system_cleanup();
    gpio_cleanup();
    logger_cleanup();
    
    LOG_INFO("MAIN", "Servidor térreo finalizado");
    
    return 0;
}