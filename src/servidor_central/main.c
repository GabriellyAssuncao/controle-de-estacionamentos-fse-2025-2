#include "parking_system.h"
#include "system_logger.h"
#include "gpio_control.h"
#include "gate_control.h"
#include "parking_logic.h"
#include "modbus_client.h"
#include "tcp_communication.h"
#include <signal.h>

static volatile bool running = true;

// Estado global do estacionamento
static parking_status_t g_parking_status;

// mutex para recursos compartilhados
static pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ========================================================================== */
static void handle_signal(int sig) {
    (void)sig;
    running = false;
    LOG_WARN("MAIN", "Sinal de término recebido. Encerrando...");
}

/* ========================================================================== */
static void print_menu(void) {
    printf("\n====== SERVIDOR CENTRAL - MENU ======\n");
    printf("1 - Status geral das vagas\n");
    printf("2 - Listar vagas por andar\n");
    printf("3 - Bloquear/Desbloquear andar\n");
    printf("4 - Abrir cancela de entrada\n");
    printf("5 - Fechar cancela de entrada\n");
    printf("6 - Abrir cancela de saída\n");
    printf("7 - Fechar cancela de saída\n");
    printf("0 - Sair\n");
    printf("Selecione: ");
    fflush(stdout);
}

/* ========================================================================== */
static void cmd_show_status(void) {
    pthread_mutex_lock(&status_mutex);
    parking_print_status(&g_parking_status); 
    pthread_mutex_unlock(&status_mutex);
}

static void cmd_list_floor_spots(void) {
    int floor;
    printf("Andar (0-%d): ", MAX_FLOORS - 1);
    if (scanf("%d", &floor) != 1) { 
        while (getchar()!='\n'); 
        return; 
    }
    if (floor < 0 || floor >= MAX_FLOORS) {
        printf("Andar inválido.\n");
        return;
    }
    
    pthread_mutex_lock(&status_mutex);
    
    // CORRIGIDO: usar total_free em vez de free_spots
    floor_status_t *fs = &g_parking_status.floors[floor];
    printf("-- Andar %d -- Livre: %u  Bloqueado: %s\n", 
           floor, fs->total_free, fs->blocked?"SIM":"NÃO");
    
    printf("   Por tipo: %u PNE | %u Idoso+ | %u Comuns\n",
           fs->free_pne, fs->free_idoso, fs->free_comum);
    
    printf("   Carros: %u\n\n", fs->cars_count);
    
    for (int i = 0; i < fs->num_spots; i++) {
        parking_spot_t *sp = &fs->spots[i];
        printf("  Vaga %d (%s): %s", 
               i, 
               spot_type_to_string(sp->type),
               sp->occupied ? "OCUPADA" : "LIVRE");
        
        if (sp->occupied && strlen(sp->plate) > 0) {
            printf(" - Placa: %s", sp->plate);
        }
        printf("\n");
    }
    
    pthread_mutex_unlock(&status_mutex);
}

static void cmd_toggle_block_floor(void) {
    int floor; 
    printf("Andar para (des)bloquear (0-%d): ", MAX_FLOORS - 1);
    if (scanf("%d", &floor) != 1) { 
        while (getchar()!='\n'); 
        return; 
    }
    if (floor < 0 || floor >= MAX_FLOORS) { 
        printf("Andar inválido.\n"); 
        return; 
    }
    
    pthread_mutex_lock(&status_mutex);
    bool blocked = g_parking_status.floors[floor].blocked;
    parking_set_floor_blocked(&g_parking_status, (floor_id_t)floor, !blocked);
    pthread_mutex_unlock(&status_mutex);
    
    LOG_INFO("MAIN", "Andar %d agora %s", floor, blocked?"DESBLOQUEADO":"BLOQUEADO");
}

static void cmd_gate_action(gate_type_t type, bool open) {
    int r = open ? gate_open(type) : gate_close(type);
    if (r == 0) {
        LOG_INFO("GATE", "Cancela %s %s", 
                 type==GATE_ENTRY?"ENTRADA":"SAÍDA", 
                 open?"abrindo":"fechando");
    } else {
        LOG_ERROR("GATE", "Falha ao %s cancela", open?"abrir":"fechar");
    }
}

/* ========================================================================== */
int main(void) {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    if (logger_init(LOG_DIR) != 0) {
        fprintf(stderr, "Falha ao iniciar logger.\n");
        return 1;
    }
    logger_set_level(LOG_LEVEL_DEBUG);

    LOG_INFO("MAIN", "Servidor Central iniciando - versão %s", SYSTEM_VERSION);

    // Inicializa GPIO
    if (gpio_init() != 0) {
        LOG_WARN("GPIO", "Falha ao inicializar GPIO (modo MOCK ou erro)");
    }

    // Inicializa sistema de cancelas
    if (gate_system_init() != 0) {
        LOG_WARN("GATE", "Falha ao inicializar sistema de cancelas");
    }

    // Inicializa lógica de estacionamento
    parking_init(&g_parking_status);

    // TODO: iniciar servidor TCP para receber eventos dos andares
    // int server_socket = tcp_server_init(SERVER_CENTRAL_PORT);
    // if (server_socket < 0) { 
    //     LOG_ERROR("TCP", "Falha ao iniciar servidor TCP"); 
    // }

    // Loop principal
    while (running) {
        print_menu();
        int opt;
        if (scanf("%d", &opt) != 1) {
            while (getchar()!='\n');
            continue;
        }
        
        switch (opt) {
            case 1: 
                cmd_show_status(); 
                break;
            case 2: 
                cmd_list_floor_spots(); 
                break;
            case 3: 
                cmd_toggle_block_floor(); 
                break;
            case 4: 
                cmd_gate_action(GATE_ENTRY, true); 
                break;
            case 5: 
                cmd_gate_action(GATE_ENTRY, false); 
                break;
            case 6: 
                cmd_gate_action(GATE_EXIT, true); 
                break;
            case 7: 
                cmd_gate_action(GATE_EXIT, false); 
                break;
            case 0: 
                running = false; 
                break;
            default: 
                printf("Opção inválida.\n"); 
                break;
        }
    }

    LOG_INFO("MAIN", "Encerrando servidor central...");

    // Cleanup
    gate_system_cleanup();
    gpio_cleanup();
    logger_cleanup();
    
    return 0;
}