#include "modbus_client.h"
#include "system_logger.h"
#ifdef MOCK_BUILD
int modbus_init(const char* device,int baud){LOG_INFO("MODBUS-MOCK","init %s %d",device,baud);return 0;}
void modbus_cleanup(void){LOG_INFO("MODBUS-MOCK","cleanup");}
int modbus_trigger_camera(uint8_t addr){LOG_INFO("MODBUS-MOCK","trigger cam %u",addr);return 0;}
int modbus_read_plate(uint8_t addr,char* plate,int* conf){(void)addr;if(plate){snprintf(plate,9,"AAA1234");}if(conf)*conf=99;return 0;}
int modbus_update_display(uint8_t a,uint8_t b,uint8_t c,uint8_t d,uint16_t f){LOG_INFO("MODBUS-MOCK","display %u %u %u %u flags=%u",a,b,c,d,f);return 0;}
#endif
