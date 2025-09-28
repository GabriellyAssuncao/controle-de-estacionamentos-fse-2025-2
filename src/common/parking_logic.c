/**
 * @file parking_logic.c
 * @brief Lógica de negócio para controle das vagas e operações do estacionamento
 */

#include "parking_logic.h"
#include "system_logger.h"
#include "gpio_control.h"
#include <string.h>

// =============================================================================
// FUNÇÕES DE INICIALIZAÇÃO
// =============================================================================

void parking_init(parking_status_t* status) {
    if (!status) return;
    
    LOG_INFO("PARKING", "Inicializando sistema de estacionamento...");
    
    memset(status, 0, sizeof(parking_status_t));
    
    // Inicializa todos os andares
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        status->floors[floor].free_spots = MAX_PARKING_SPOTS_PER_FLOOR;
        status->floors[floor].blocked = false;
        
        // Inicializa todas as vagas como livres
        for (int spot = 0; spot < MAX_PARKING_SPOTS_PER_FLOOR; spot++) {
            status->floors[floor].spots[spot].occupied = false;
            status->floors[floor].spots[spot].timestamp = time(NULL);
            strcpy(status->floors[floor].spots[spot].plate, "");
        }
    }
    
    status->total_free_spots = TOTAL_PARKING_SPOTS;
    status->system_full = false;
    status->emergency_mode = false;
    
    LOG_INFO("PARKING", "Sistema inicializado - Total de vagas: %d", TOTAL_PARKING_SPOTS);
}

// =============================================================================
// VARREDURA DE VAGAS
// =============================================================================

int parking_scan_floor(floor_id_t floor_id, const gpio_floor_config_t* config,
                       floor_status_t* floor_status) {
    if (!config || !floor_status || floor_id >= MAX_FLOORS) {
        return -1;
    }
    
    uint8_t occupied_count = 0;
    uint8_t changes_detected = 0;
    
    LOG_DEBUG("PARKING", "Iniciando varredura do andar %d", floor_id);
    
    // Varre todas as 8 vagas do andar
    for (uint8_t spot = 0; spot < MAX_PARKING_SPOTS_PER_FLOOR; spot++) {
        // Configura o endereço da vaga
        if (gpio_set_address(config, spot) != 0) {
            LOG_ERROR("PARKING", "Erro ao configurar endereço %d no andar %d", spot, floor_id);
            continue;
        }
        
        // Lê o sensor da vaga
        bool currently_occupied = gpio_read_parking_sensor(config);
        bool was_occupied = floor_status->spots[spot].occupied;
        
        // Detecta mudança de estado
        if (currently_occupied != was_occupied) {
            changes_detected++;
            
            time_t now = time(NULL);
            floor_status->spots[spot].occupied = currently_occupied;
            floor_status->spots[spot].timestamp = now;
            
            char time_str[32];
            time_to_string(now, time_str, sizeof(time_str));
            
            LOG_INFO("PARKING", "Andar %d, Vaga %d: %s -> %s [%s]",
                     floor_id, spot,
                     was_occupied ? "OCUPADA" : "LIVRE",
                     currently_occupied ? "OCUPADA" : "LIVRE",
                     time_str);
                     
            // Se vaga foi ocupada, limpa a placa (será preenchida depois)
            if (currently_occupied && !was_occupied) {
                strcpy(floor_status->spots[spot].plate, "");
            }
        }
        
        if (currently_occupied) {
            occupied_count++;
        }
    }
    
    // Atualiza contador de vagas livres
    uint8_t new_free_spots = MAX_PARKING_SPOTS_PER_FLOOR - occupied_count;
    
    if (floor_status->free_spots != new_free_spots) {
        LOG_INFO("PARKING", "Andar %d: Vagas livres %d -> %d", 
                 floor_id, floor_status->free_spots, new_free_spots);
        floor_status->free_spots = new_free_spots;
    }
    
    LOG_DEBUG("PARKING", "Varredura andar %d concluída - %d mudanças detectadas", 
              floor_id, changes_detected);
    
    return changes_detected;
}

// =============================================================================
// ALOCAÇÃO E LIBERAÇÃO DE VAGAS
// =============================================================================

bool parking_allocate_spot(parking_status_t* status, const char* plate, floor_id_t preferred_floor) {
    if (!status || !plate || !is_valid_plate(plate)) {
        return false;
    }
    
    LOG_INFO("PARKING", "Tentando alocar vaga para placa %s (andar preferido: %d)", 
             plate, preferred_floor);
    
    // Verifica se sistema está lotado
    if (status->system_full) {
        LOG_WARN("PARKING", "Sistema lotado - recusando entrada da placa %s", plate);
        return false;
    }
    
    // Tenta primeiro o andar preferido
    for (int floor = preferred_floor; floor < MAX_FLOORS; floor++) {
        if (status->floors[floor].blocked || status->floors[floor].free_spots == 0) {
            continue;
        }
        
        // Procura primeira vaga livre neste andar
        for (int spot = 0; spot < MAX_PARKING_SPOTS_PER_FLOOR; spot++) {
            if (!status->floors[floor].spots[spot].occupied) {
                // Aloca a vaga
                status->floors[floor].spots[spot].occupied = true;
                status->floors[floor].spots[spot].timestamp = time(NULL);
                strcpy(status->floors[floor].spots[spot].plate, plate);
                
                // Atualiza contadores
                status->floors[floor].free_spots--;
                status->total_free_spots--;
                
                // Verifica se ficou lotado
                if (status->total_free_spots == 0) {
                    status->system_full = true;
                    LOG_WARN("PARKING", "Sistema ficou LOTADO!");
                }
                
                LOG_INFO("PARKING", "Vaga alocada: Andar %d, Spot %d para placa %s", 
                         floor, spot, plate);
                
                return true;
            }
        }
    }
    
    // Se não encontrou no andar preferido, tenta outros andares
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        // Cast explícito para evitar warning de signed/unsigned ou enum/int
        if ((floor_id_t)floor == preferred_floor) continue; // Já tentou
        
        if (status->floors[floor].blocked || status->floors[floor].free_spots == 0) {
            continue;
        }
        
        for (int spot = 0; spot < MAX_PARKING_SPOTS_PER_FLOOR; spot++) {
            if (!status->floors[floor].spots[spot].occupied) {
                // Aloca a vaga
                status->floors[floor].spots[spot].occupied = true;
                status->floors[floor].spots[spot].timestamp = time(NULL);
                strcpy(status->floors[floor].spots[spot].plate, plate);
                
                // Atualiza contadores
                status->floors[floor].free_spots--;
                status->total_free_spots--;
                
                if (status->total_free_spots == 0) {
                    status->system_full = true;
                    LOG_WARN("PARKING", "Sistema ficou LOTADO!");
                }
                
                LOG_INFO("PARKING", "Vaga alocada: Andar %d, Spot %d para placa %s", 
                         floor, spot, plate);
                
                return true;
            }
        }
    }
    
    LOG_WARN("PARKING", "Não foi possível alocar vaga para placa %s", plate);
    return false;
}

bool parking_free_spot(parking_status_t* status, const char* plate) {
    if (!status || !plate || !is_valid_plate(plate)) {
        return false;
    }
    
    LOG_INFO("PARKING", "Tentando liberar vaga da placa %s", plate);
    
    // Procura a vaga da placa em todos os andares
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        for (int spot = 0; spot < MAX_PARKING_SPOTS_PER_FLOOR; spot++) {
            parking_spot_t* current_spot = &status->floors[floor].spots[spot];
            
            if (current_spot->occupied && strcmp(current_spot->plate, plate) == 0) {
                // Libera a vaga
                current_spot->occupied = false;
                current_spot->timestamp = time(NULL);
                strcpy(current_spot->plate, "");
                
                // Atualiza contadores
                status->floors[floor].free_spots++;
                status->total_free_spots++;
                
                // Sistema não está mais lotado
                if (status->system_full && status->total_free_spots > 0) {
                    status->system_full = false;
                    LOG_INFO("PARKING", "Sistema não está mais lotado");
                }
                
                LOG_INFO("PARKING", "Vaga liberada: Andar %d, Spot %d da placa %s", 
                         floor, spot, plate);
                
                return true;
            }
        }
    }
    
    LOG_WARN("PARKING", "Placa %s não encontrada para liberação", plate);
    return false;
}

// =============================================================================
// CÁLCULO DE TARIFAS
// =============================================================================

uint32_t parking_calculate_fee(time_t entry_time, time_t exit_time) {
    if (exit_time <= entry_time) {
        LOG_ERROR("PARKING", "Tempo de saída deve ser maior que entrada");
        return 0;
    }
    
    // Calcula diferença em segundos
    double diff_seconds = difftime(exit_time, entry_time);
    
    // Converte para minutos (arredonda para cima)
    uint32_t minutes = (uint32_t)((diff_seconds + 59) / 60); // +59 para arredondar para cima
    
    // Calcula taxa (PRICE_PER_MINUTE_CENTS centavos por minuto)
    uint32_t fee_cents = minutes * PRICE_PER_MINUTE_CENTS;
    
    LOG_INFO("PARKING", "Cálculo de tarifa: %.0f segundos = %d minutos = %d centavos", 
             diff_seconds, minutes, fee_cents);
    
    return fee_cents;
}

// =============================================================================
// FUNÇÕES DE CONTROLE DO SISTEMA
// =============================================================================

void parking_update_total_stats(parking_status_t* status) {
    if (!status) return;
    
    uint8_t total_free = 0;
    
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        total_free += status->floors[floor].free_spots;
    }
    
    status->total_free_spots = total_free;
    status->system_full = (total_free == 0);
}

void parking_set_floor_blocked(parking_status_t* status, floor_id_t floor_id, bool blocked) {
    if (!status || floor_id >= MAX_FLOORS) return;
    
    status->floors[floor_id].blocked = blocked;
    
    LOG_INFO("PARKING", "Andar %d %s", floor_id, blocked ? "BLOQUEADO" : "DESBLOQUEADO");
    
    // Atualiza estatísticas
    parking_update_total_stats(status);
}

void parking_set_emergency_mode(parking_status_t* status, bool emergency) {
    if (!status) return;
    
    status->emergency_mode = emergency;
    
    if (emergency) {
        LOG_WARN("PARKING", "MODO DE EMERGÊNCIA ATIVADO");
        // Em modo de emergência, libera todas as cancelas
    } else {
        LOG_INFO("PARKING", "Modo de emergência desativado");
    }
}

// =============================================================================
// FUNÇÕES DE DEBUG E RELATÓRIOS
// =============================================================================

void parking_print_status(const parking_status_t* status) {
    if (!status) return;
    
    printf("\n=== STATUS DO ESTACIONAMENTO ===\n");
    printf("Total de vagas livres: %d/%d\n", status->total_free_spots, TOTAL_PARKING_SPOTS);
    printf("Sistema lotado: %s\n", status->system_full ? "SIM" : "NÃO");
    printf("Modo de emergência: %s\n", status->emergency_mode ? "SIM" : "NÃO");
    
    const char* floor_names[] = {"TÉRREO", "1º ANDAR", "2º ANDAR"};
    
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        printf("\n%s:\n", floor_names[floor]);
        printf("  Vagas livres: %d/%d\n", 
               status->floors[floor].free_spots, MAX_PARKING_SPOTS_PER_FLOOR);
        printf("  Bloqueado: %s\n", status->floors[floor].blocked ? "SIM" : "NÃO");
        
        printf("  Vagas: ");
        for (int spot = 0; spot < MAX_PARKING_SPOTS_PER_FLOOR; spot++) {
            if (status->floors[floor].spots[spot].occupied) {
                printf("[X]");
            } else {
                printf("[ ]");
            }
        }
        printf("\n");
    }
    
    printf("================================\n\n");
}