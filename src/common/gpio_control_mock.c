#include "gpio_control.h"
#include "system_logger.h"
#ifdef MOCK_BUILD
static bool initialized = false;
int gpio_init(void){initialized=true;LOG_INFO("GPIO-MOCK","init");return 0;}
void gpio_cleanup(void){initialized=false;LOG_INFO("GPIO-MOCK","cleanup");}
int gpio_set_address(const gpio_floor_config_t* config, uint8_t address){(void)config;(void)address;return 0;}
bool gpio_read_parking_sensor(const gpio_floor_config_t* config){(void)config;return false;}
bool gpio_read_gate_sensor(uint8_t pin){(void)pin;return false;}
void gpio_set_gate_motor(uint8_t pin,bool activate){LOG_DEBUG("GPIO-MOCK","motor pin %u -> %d",pin,activate);}
void gpio_test_all_pins(void){LOG_INFO("GPIO-MOCK","test all pins");}
#endif
