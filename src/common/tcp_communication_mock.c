#include "tcp_communication.h"
#include "system_logger.h"
#ifdef MOCK_BUILD
int tcp_server_init(int port){LOG_INFO("TCP-MOCK","server init %d",port);return 1;}
int tcp_client_connect(const char* host,int port){LOG_INFO("TCP-MOCK","connect %s:%d",host,port);return 2;}
int tcp_send_message(int socket,const system_message_t* msg){(void)socket;(void)msg;LOG_DEBUG("TCP-MOCK","send message stub");return 0;}
int tcp_receive_message(int socket, system_message_t* msg){(void)socket;(void)msg;return -1;}
void tcp_close_connection(int socket){(void)socket;LOG_INFO("TCP-MOCK","close");}
#endif
