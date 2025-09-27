/**
 * @file tcp_communication.h
 * @brief Header da comunicação TCP/IP
 */

#ifndef TCP_COMMUNICATION_H
#define TCP_COMMUNICATION_H

#include "parking_system.h"

/**
 * @brief Inicializa servidor TCP
 * @param port Porta para escutar
 * @return Socket do servidor ou -1 se erro
 */
int tcp_server_init(int port);

/**
 * @brief Conecta a um servidor TCP
 * @param host Endereço do host
 * @param port Porta do servidor
 * @return Socket da conexão ou -1 se erro
 */
int tcp_client_connect(const char* host, int port);

/**
 * @brief Envia mensagem via TCP
 * @param socket Socket da conexão
 * @param msg Mensagem a ser enviada
 * @return 0 se sucesso, -1 se erro
 */
int tcp_send_message(int socket, const system_message_t* msg);

/**
 * @brief Recebe mensagem via TCP
 * @param socket Socket da conexão
 * @param msg Buffer para mensagem recebida
 * @return 0 se sucesso, -1 se erro
 */
int tcp_receive_message(int socket, system_message_t* msg);

/**
 * @brief Fecha conexão TCP
 * @param socket Socket da conexão
 */
void tcp_close_connection(int socket);

#endif // TCP_COMMUNICATION_H