/**
 * @file tcp_communication.c
 * @brief Sistema de comunicação TCP/IP entre servidores
 */

#include "tcp_communication.h"
#include "system_logger.h"

// Implementação temporária básica - será expandida depois
int tcp_server_init(int port) {
    (void)port;
    LOG_INFO("TCP", "TCP server inicializado (stub)");
    return 0;
}

int tcp_client_connect(const char* host, int port) {
    (void)host;
    (void)port;
    return 0;
}

int tcp_send_message(int socket, const system_message_t* msg) {
    (void)socket;
    (void)msg;
    return 0;
}

int tcp_receive_message(int socket, system_message_t* msg) {
    (void)socket;
    (void)msg;
    return 0;
}

void tcp_close_connection(int socket) {
    (void)socket;
}