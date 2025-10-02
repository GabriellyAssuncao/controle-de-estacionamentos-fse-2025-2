/**
 * @file parking_logic.c
 * @brief Lógica de negócio para controle das vagas e operações do estacionamento
 */

#include "parking_logic.h"
#include "system_logger.h"
#include "gpio_control.h"
#include <string.h>

static const spot_type_t TERREO_SPOT_TYPES[SPOTS_TERREO] = {
    SPOT_TYPE_PNE,      
    SPOT_TYPE_IDOSO,    
    SPOT_TYPE_COMUM,    
    SPOT_TYPE_COMUM     
};
)
static const spot_type_t ANDAR1_SPOT_TYPES[SPOTS_ANDAR1] = {
    SPOT_TYPE_PNE,      
    SPOT_TYPE_PNE,      
    SPOT_TYPE_IDOSO,    
    SPOT_TYPE_COMUM,    
    SPOT_TYPE_COMUM,    
    SPOT_TYPE_COMUM,    
    SPOT_TYPE_COMUM     
}; 
static const spot_type_t ANDAR2_SPOT_TYPES[SPOTS_ANDAR2] = {
    SPOT_TYPE_PNE,       
    SPOT_TYPE_PNE,       
    SPOT_TYPE_IDOSO,     
    SPOT_TYPE_IDOSO,     
    SPOT_TYPE_COMUM,     
    SPOT_TYPE_COMUM,     
    SPOT_TYPE_COMUM,     
    SPOT_TYPE_COMUM      
};
 
/**
 * @brief Obtém o tipo de uma vaga baseado no andar e índice
 */
static spot_type_t get_spot_type(floor_id_t floor_id, uint8_t spot_index) {
    switch(floor_id) {
        case FLOOR_TERREO:
            if (spot_index < SPOTS_TERREO) {
                return TERREO_SPOT_TYPES[spot_index];
            }
            break;
        case FLOOR_ANDAR1:
            if (spot_index < SPOTS_ANDAR1) {
                return ANDAR1_SPOT_TYPES[spot_index];
            }
            break;
        case FLOOR_ANDAR2:
            if (spot_index < SPOTS_ANDAR2) {
                return ANDAR2_SPOT_TYPES[spot_index];
            }
            break;
    }
    return SPOT_TYPE_COMUM;  
}

/**
 * @brief Atualiza contadores de vagas livres por tipo em um andar
 */
static void update_floor_counters(floor_status_t* floor) {
    if (!floor) return;
    
    floor->free_pne = 0;
    floor->free_idoso = 0;
    floor->free_comum = 0;
    floor->cars_count = 0;
    
    for (int i = 0; i < floor->num_spots; i++) {
        parking_spot_t* spot = &floor->spots[i];
        
        if (spot->occupied) {
            floor->cars_count++;
        } else {
             switch(spot->type) {
                case SPOT_TYPE_PNE:
                    floor->free_pne++;
                    break;
                case SPOT_TYPE_IDOSO:
                    floor->free_idoso++;
                    break;
                case SPOT_TYPE_COMUM:
                    floor->free_comum++;
                    break;
            }
        }
    }
    
    floor->total_free = floor->free_pne + floor->free_idoso + floor->free_comum;
}
 
void parking_init(parking_status_t* status) {
    if (!status) return;
    
    LOG_INFO("PARKING", "Inicializando sistema de estacionamento...");
    
    memset(status, 0, sizeof(parking_status_t));
    
     
    const uint8_t spots_per_floor[MAX_FLOORS] = {SPOTS_TERREO, SPOTS_ANDAR1, SPOTS_ANDAR2};
    
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        floor_status_t* f = &status->floors[floor];
        f->num_spots = spots_per_floor[floor];
        f->blocked = false;
        
        for (int spot = 0; spot < f->num_spots; spot++) {
            f->spots[spot].occupied = false;
            f->spots[spot].type = get_spot_type((floor_id_t)floor, spot);
            f->spots[spot].timestamp = time(NULL);
            f->spots[spot].confidence = 0;
            strcpy(f->spots[spot].plate, "");
        }
        
        update_floor_counters(f);
        
        LOG_INFO("PARKING", "Andar %d: %d vagas (%d PNE, %d Idoso+, %d Comuns)", 
                 floor, f->num_spots, f->free_pne, f->free_idoso, f->free_comum);
    }
    
    parking_update_total_stats(status);
    
    LOG_INFO("PARKING", "Sistema inicializado - Total: %d vagas (%d PNE, %d Idoso+, %d Comuns)", 
             TOTAL_PARKING_SPOTS,
             status->total_free_pne,
             status->total_free_idoso,
             status->total_free_comum);
}
 
int parking_scan_floor(floor_id_t floor_id, const gpio_floor_config_t* config,
                       floor_status_t* floor_status) {
    if (!config || !floor_status || floor_id >= MAX_FLOORS) {
        return -1;
    }
    
    uint8_t changes_detected = 0;
    
    LOG_DEBUG("PARKING", "Iniciando varredura do andar %d (%d vagas)", 
              floor_id, floor_status->num_spots);
    
     
    for (uint8_t spot = 0; spot < floor_status->num_spots; spot++) {
         
        if (gpio_set_address(config, spot) != 0) {
            LOG_ERROR("PARKING", "Erro ao configurar endereço %d no andar %d", spot, floor_id);
            continue;
        }
        
         
        bool currently_occupied = gpio_read_parking_sensor(config);
        bool was_occupied = floor_status->spots[spot].occupied;
        
         
        if (currently_occupied != was_occupied) {
            changes_detected++;
            
            time_t now = time(NULL);
            floor_status->spots[spot].occupied = currently_occupied;
            floor_status->spots[spot].timestamp = now;
            
            char time_str[32];
            time_to_string(now, time_str, sizeof(time_str));
            
            const char* type_str = spot_type_to_string(floor_status->spots[spot].type);
            
            LOG_INFO("PARKING", "Andar %d, Vaga %d (%s): %s -> %s [%s]",
                     floor_id, spot, type_str,
                     was_occupied ? "OCUPADA" : "LIVRE",
                     currently_occupied ? "OCUPADA" : "LIVRE",
                     time_str);
                     
             if (currently_occupied && !was_occupied) {
                strcpy(floor_status->spots[spot].plate, "");
                floor_status->spots[spot].confidence = 0;
            }
        }
    }
    
     if (changes_detected > 0) {
        update_floor_counters(floor_status);
        
        LOG_INFO("PARKING", "Andar %d: PNE=%d, Idoso+=%d, Comuns=%d, Total=%d livres (%d carros)", 
                 floor_id,
                 floor_status->free_pne,
                 floor_status->free_idoso,
                 floor_status->free_comum,
                 floor_status->total_free,
                 floor_status->cars_count);
    }
    
    LOG_DEBUG("PARKING", "Varredura andar %d concluída - %d mudanças detectadas", 
              floor_id, changes_detected);
    
    return changes_detected;
}
 
bool parking_allocate_spot(parking_status_t* status, const char* plate, 
                           spot_type_t preferred_type, floor_id_t preferred_floor) {
    if (!status || !plate || !is_valid_plate(plate)) {
        return false;
    }
    
    LOG_INFO("PARKING", "Tentando alocar vaga %s para placa %s (andar preferido: %d)", 
             spot_type_to_string(preferred_type), plate, preferred_floor);
    
    if (status->system_full) {
        LOG_WARN("PARKING", "Sistema lotado - recusando entrada da placa %s", plate);
        return false;
    }
    
    spot_type_t types_to_try[3];
    int num_types = 0;
    
    types_to_try[num_types++] = preferred_type;
    
    if (preferred_type != SPOT_TYPE_COMUM) {
        types_to_try[num_types++] = SPOT_TYPE_COMUM;
    }
    if (preferred_type != SPOT_TYPE_IDOSO) {
        types_to_try[num_types++] = SPOT_TYPE_IDOSO;
    }
    if (preferred_type != SPOT_TYPE_PNE) {
        types_to_try[num_types++] = SPOT_TYPE_PNE;
    }
    
    for (int floor_offset = 0; floor_offset < MAX_FLOORS; floor_offset++) {
        int floor = (preferred_floor + floor_offset) % MAX_FLOORS;
        
        if (status->floors[floor].blocked) {
            continue;
        }
        
        for (int t = 0; t < num_types; t++) {
            spot_type_t try_type = types_to_try[t];
            
            for (int spot = 0; spot < status->floors[floor].num_spots; spot++) {
                parking_spot_t* s = &status->floors[floor].spots[spot];
                
                if (!s->occupied && s->type == try_type) {
                    
                    s->occupied = true;
                    s->timestamp = time(NULL);
                    strcpy(s->plate, plate);
                    s->confidence = 0; // Será atualizado depois
                    
                    // Atualiza contadores
                    update_floor_counters(&status->floors[floor]);
                    parking_update_total_stats(status);
                    
                    LOG_INFO("PARKING", "Vaga alocada: Andar %d, Spot %d (%s) para placa %s", 
                             floor, spot, spot_type_to_string(try_type), plate);
                    
                    return true;
                }
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
        for (int spot = 0; spot < status->floors[floor].num_spots; spot++) {
            parking_spot_t* current_spot = &status->floors[floor].spots[spot];
            
            if (current_spot->occupied && strcmp(current_spot->plate, plate) == 0) {
                // Libera a vaga
                current_spot->occupied = false;
                current_spot->timestamp = time(NULL);
                strcpy(current_spot->plate, "");
                current_spot->confidence = 0;
                
                // Atualiza contadores
                update_floor_counters(&status->floors[floor]);
                parking_update_total_stats(status);
                
                LOG_INFO("PARKING", "Vaga liberada: Andar %d, Spot %d (%s) da placa %s", 
                         floor, spot, spot_type_to_string(current_spot->type), plate);
                
                return true;
            }
        }
    }
    
    LOG_WARN("PARKING", "Placa %s não encontrada para liberação", plate);
    return false;
}

uint32_t parking_calculate_fee(time_t entry_time, time_t exit_time) {
    if (exit_time <= entry_time) {
        LOG_ERROR("PARKING", "Tempo de saída deve ser maior que entrada");
        return 0;
    }
    
    double diff_seconds = difftime(exit_time, entry_time);
    
    uint32_t minutes = (uint32_t)((diff_seconds + 59) / 60);
    
    uint32_t fee_cents = minutes * PRICE_PER_MINUTE_CENTS;
    
    char money_str[32];
    format_money(fee_cents, money_str, sizeof(money_str));
    
    LOG_INFO("PARKING", "Cálculo de tarifa: %.0f segundos = %u minutos = %s", 
             diff_seconds, minutes, money_str);
    
    return fee_cents;
}

void parking_update_total_stats(parking_status_t* status) {
    if (!status) return;
    
    status->total_free_pne = 0;
    status->total_free_idoso = 0;
    status->total_free_comum = 0;
    status->total_free_spots = 0;
    status->total_cars = 0;
    
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        status->total_free_pne += status->floors[floor].free_pne;
        status->total_free_idoso += status->floors[floor].free_idoso;
        status->total_free_comum += status->floors[floor].free_comum;
        status->total_free_spots += status->floors[floor].total_free;
        status->total_cars += status->floors[floor].cars_count;
    }
    
    status->system_full = (status->total_free_spots == 0);
    
    if (status->system_full) {
        LOG_WARN("PARKING", "ESTACIONAMENTO LOTADO!");
    }
}

void parking_set_floor_blocked(parking_status_t* status, floor_id_t floor_id, bool blocked) {
    if (!status || floor_id >= MAX_FLOORS) return;
    
    status->floors[floor_id].blocked = blocked;
    
    LOG_INFO("PARKING", "Andar %d %s", floor_id, blocked ? "BLOQUEADO" : "DESBLOQUEADO");
    
    parking_update_total_stats(status);
}

void parking_set_emergency_mode(parking_status_t* status, bool emergency) {
    if (!status) return;
    
    status->emergency_mode = emergency;
    
    if (emergency) {
        LOG_WARN("PARKING", "MODO DE EMERGÊNCIA ATIVADO");
    } else {
        LOG_INFO("PARKING", "Modo de emergência desativado");
    }
}

void parking_print_status(const parking_status_t* status) {
    if (!status) return;
    
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║           STATUS DO ESTACIONAMENTO                            ║\n");
    printf("╠════════════════════════════════════════════════════════════════╣\n");
    
    printf("║ TOTAL GERAL                                                    ║\n");
    printf("║   Vagas Livres:  %2d PNE | %2d Idoso+ | %2d Comuns = %2d total  ║\n",
           status->total_free_pne, status->total_free_idoso, 
           status->total_free_comum, status->total_free_spots);
    printf("║   Carros:        %2d                                            ║\n",
           status->total_cars);
    printf("║   Status:        %-45s ║\n", 
           status->system_full ? "LOTADO" : "Disponível");
    printf("║   Emergência:    %-45s ║\n",
           status->emergency_mode ? "ATIVO" : "Normal");
    
    const char* floor_names[] = {"TÉRREO", "1º ANDAR", "2º ANDAR"};
    
    for (int floor = 0; floor < MAX_FLOORS; floor++) {
        const floor_status_t* f = &status->floors[floor];
        
        printf("╠────────────────────────────────────────────────────────────────╣\n");
        printf("║ %-62s ║\n", floor_names[floor]);
        printf("║   Vagas Livres:  %2d PNE | %2d Idoso+ | %2d Comuns = %2d total  ║\n",
               f->free_pne, f->free_idoso, f->free_comum, f->total_free);
        printf("║   Carros:        %2d                                            ║\n",
               f->cars_count);
        printf("║   Bloqueado:     %-45s ║\n", f->blocked ? "SIM" : "NÃO");
        
        printf("║   Mapa:          ");
        for (int spot = 0; spot < f->num_spots; spot++) {
            if (f->spots[spot].occupied) {
                printf("[X]");
            } else {
                switch(f->spots[spot].type) {
                    case SPOT_TYPE_PNE: printf("[P]"); break;
                    case SPOT_TYPE_IDOSO: printf("[I]"); break;
                    case SPOT_TYPE_COMUM: printf("[ ]"); break;
                }
            }
        }
        for (int i = f->num_spots; i < MAX_PARKING_SPOTS_PER_FLOOR; i++) {
            printf("   ");
        }
        printf("        ║\n");
    }
    
    printf("╚════════════════════════════════════════════════════════════════╝\n");
    printf("Legenda: [X]=Ocupada [P]=PNE [ ]=Comum [I]=Idoso+\n\n");
}

void parking_print_floor_details(const parking_status_t* status, floor_id_t floor_id) {
    if (!status || floor_id >= MAX_FLOORS) return;
    
    const floor_status_t* f = &status->floors[floor_id];
    const char* floor_names[] = {"TÉRREO", "1º ANDAR", "2º ANDAR"};
    
    printf("\n=== DETALHES DO %s ===\n", floor_names[floor_id]);
    printf("Total de vagas: %d\n", f->num_spots);
    printf("Vagas livres: %d PNE, %d Idoso+, %d Comuns\n",
           f->free_pne, f->free_idoso, f->free_comum);
    printf("Carros: %d\n", f->cars_count);
    printf("Bloqueado: %s\n\n", f->blocked ? "SIM" : "NÃO");
    
    printf("%-5s %-10s %-10s %-10s %-20s\n", 
           "Vaga", "Tipo", "Status", "Placa", "Última Atualização");
    printf("----------------------------------------------------------------\n");
    
    for (int i = 0; i < f->num_spots; i++) {
        const parking_spot_t* spot = &f->spots[i];
        char time_str[32];
        time_to_string(spot->timestamp, time_str, sizeof(time_str));
        
        printf("%-5d %-10s %-10s %-10s %s\n",
               i,
               spot_type_to_string(spot->type),
               spot->occupied ? "OCUPADA" : "LIVRE",
               spot->occupied ? spot->plate : "-",
               time_str);
    }
    printf("\n");
}