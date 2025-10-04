#ifndef PARKING_LOGIC_H
#define PARKING_LOGIC_H

#include "parking_system.h"

void parking_init(parking_status_t* status);

int parking_scan_floor(floor_id_t floor_id, const gpio_floor_config_t* config,
                       floor_status_t* floor_status);

bool parking_allocate_spot(parking_status_t* status, const char* plate,
                           spot_type_t preferred_type, floor_id_t preferred_floor);

bool parking_free_spot(parking_status_t* status, const char* plate);

uint32_t parking_calculate_fee(time_t entry_time, time_t exit_time);

void parking_update_total_stats(parking_status_t* status);

void parking_set_floor_blocked(parking_status_t* status, floor_id_t floor_id, bool blocked);

void parking_set_emergency_mode(parking_status_t* status, bool emergency);

void parking_print_status(const parking_status_t* status);

void parking_print_floor_details(const parking_status_t* status, floor_id_t floor_id);

#endif