/**
 * @file tcp_communication.c
 * @brief Implementação de comunicação TCP usando libevent (sem JSON)
 * 
 * Este módulo implementa comunicação assíncrona entre servidores para:
 * - Sincronização de dados de estacionamento
 * - Notificações de eventos  
 * - Comandos de controle remoto
 * - Status do sistema distribuído
 */

#include "tcp_communication.h"
#include "system_logger.h"
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

// =============================================================================
// DEFINIÇÕES GLOBAIS
// =============================================================================

// Base de eventos libevent
static struct event_base *base = NULL;

// Listener para conexões de entrada
static struct evconnlistener *listener = NULL;

// Array de conexões ativas
static tcp_connection_t active_connections[MAX_CONNECTIONS];
static int connection_count = 0;

// Estado de inicialização
static bool tcp_initialized = false;

// Callbacks do usuário
static tcp_message_callback_t message_callback = NULL;
static tcp_connection_callback_t connection_callback = NULL;

// Mutex para thread safety
static pthread_mutex_t tcp_mutex = PTHREAD_MUTEX_INITIALIZER;

// =============================================================================
// FUNÇÕES PRIVADAS
// =============================================================================

/**
 * @brief Encontra uma conexão pelo bufferevent
 * @param bev BufferEvent
 * @return Ponteiro para conexão ou NULL se não encontrada
 */
static tcp_connection_t* find_connection_by_bev(struct bufferevent *bev) {
    pthread_mutex_lock(&tcp_mutex);
    for (int i = 0; i < connection_count; i++) {
        if (active_connections[i].bev == bev) {
            pthread_mutex_unlock(&tcp_mutex);
            return &active_connections[i];
        }
    }
    pthread_mutex_unlock(&tcp_mutex);
    return NULL;
}

/**
 * @brief Adiciona uma nova conexão
 * @param bev BufferEvent
 * @param address Endereço IP
 * @param port Porta
 * @param is_outgoing true se conexão sainte, false se entrada
 * @return Ponteiro para conexão ou NULL se erro
 */
static tcp_connection_t* add_connection(struct bufferevent *bev, const char *address, 
                                      int port, bool is_outgoing) {
    pthread_mutex_lock(&tcp_mutex);
    
    if (connection_count >= MAX_CONNECTIONS) {
        LOG_ERROR("TCP", "Máximo de conexões atingido");
        pthread_mutex_unlock(&tcp_mutex);
        return NULL;
    }
    
    tcp_connection_t *conn = &active_connections[connection_count++];
    conn->bev = bev;
    strncpy(conn->address, address, sizeof(conn->address) - 1);
    conn->address[sizeof(conn->address) - 1] = '\0';
    conn->port = port;
    conn->is_outgoing = is_outgoing;
    conn->connected_time = time(NULL);
    conn->last_activity = conn->connected_time;
    conn->bytes_sent = 0;
    conn->bytes_received = 0;
    
    pthread_mutex_unlock(&tcp_mutex);
    
    LOG_INFO("TCP", "Nova conexão %s: %s:%d", is_outgoing ? "sainte" : "entrada", address, port);
    
    if (connection_callback) {
        connection_callback(conn, TCP_EVENT_CONNECTED);
    }
    
    return conn;
}

/**
 * @brief Remove uma conexão
 * @param bev BufferEvent
 */
static void remove_connection(struct bufferevent *bev) {
    pthread_mutex_lock(&tcp_mutex);
    
    for (int i = 0; i < connection_count; i++) {
        if (active_connections[i].bev == bev) {
            tcp_connection_t *conn = &active_connections[i];
            
            if (connection_callback) {
                connection_callback(conn, TCP_EVENT_DISCONNECTED);
            }
            
            LOG_INFO("TCP", "Conexão removida: %s:%d", conn->address, conn->port);
            
            // Move última conexão para posição removida
            if (i < connection_count - 1) {
                active_connections[i] = active_connections[connection_count - 1];
            }
            connection_count--;
            break;
        }
    }
    
    pthread_mutex_unlock(&tcp_mutex);
}

/**
 * @brief Processa uma mensagem simples recebida (formato key=value)
 * @param conn Conexão
 * @param message_str String da mensagem
 */
static void process_simple_message(tcp_connection_t *conn, const char *message_str) {
    // Parsear mensagem simples no formato: type=parking_status,data=terreo:5,andar1:3
    char type_str[64] = {0};
    char data_str[256] = {0};
    
    // Extrair tipo
    const char *type_start = strstr(message_str, "type=");
    if (type_start) {
        type_start += 5; // pula "type="
        const char *type_end = strchr(type_start, ',');
        int type_len = type_end ? (type_end - type_start) : strlen(type_start);
        if (type_len < sizeof(type_str)) {
            strncpy(type_str, type_start, type_len);
        }
    }
    
    // Extrair dados
    const char *data_start = strstr(message_str, "data=");
    if (data_start) {
        data_start += 5; // pula "data="
        strncpy(data_str, data_start, sizeof(data_str) - 1);
    }
    
    // Determinar tipo de mensagem
    tcp_message_type_t msg_type;
    if (strcmp(type_str, "parking_status") == 0) {
        msg_type = TCP_MSG_PARKING_STATUS;
    } else if (strcmp(type_str, "vehicle_entry") == 0) {
        msg_type = TCP_MSG_VEHICLE_ENTRY;
    } else if (strcmp(type_str, "vehicle_exit") == 0) {
        msg_type = TCP_MSG_VEHICLE_EXIT;
    } else if (strcmp(type_str, "system_status") == 0) {
        msg_type = TCP_MSG_SYSTEM_STATUS;
    } else if (strcmp(type_str, "emergency") == 0) {
        msg_type = TCP_MSG_EMERGENCY;
    } else {
        LOG_WARN("TCP", "Tipo de mensagem desconhecido: %s", type_str);
        return;
    }
    
    // Criar estrutura de mensagem
    tcp_message_t message;
    message.type = msg_type;
    message.timestamp = time(NULL);
    strncpy(message.source, conn->address, sizeof(message.source) - 1);
    message.source[sizeof(message.source) - 1] = '\0';
    
    // Copiar dados
    strncpy(message.data, data_str, sizeof(message.data) - 1);
    message.data[sizeof(message.data) - 1] = '\0';
    message.data_size = strlen(message.data);
    
    // Chamar callback do usuário se definido
    if (message_callback) {
        message_callback(&message, conn);
    }
}

/**
 * @brief Callback chamado quando dados são recebidos
 * @param bev BufferEvent
 * @param user_data Dados do usuário (não usado)
 */
static void read_callback(struct bufferevent *bev, void *user_data) {
    (void)user_data; // Suprimir warning de parâmetro não usado
    
    tcp_connection_t *conn = find_connection_by_bev(bev);
    if (!conn) {
        LOG_ERROR("TCP", "Conexão não encontrada para bufferevent");
        return;
    }
    
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    
    if (len == 0) return;
    
    // Ler dados
    char *data = malloc(len + 1);
    if (!data) {
        LOG_ERROR("TCP", "Erro ao alocar memória para dados recebidos");
        return;
    }
    
    evbuffer_copyout(input, data, len);
    data[len] = '\0';
    
    // Atualizar estatísticas da conexão
    conn->last_activity = time(NULL);
    conn->bytes_received += len;
    
    // Processar mensagens (assumindo uma mensagem por linha)
    char *line_start = data;
    char *line_end;
    
    while ((line_end = strchr(line_start, '\n')) != NULL) {
        *line_end = '\0';
        
        if (strlen(line_start) > 0) {
            LOG_DEBUG("TCP", "Mensagem recebida de %s:%d: %s", conn->address, conn->port, line_start);
            process_simple_message(conn, line_start);
        }
        
        line_start = line_end + 1;
    }
    
    // Remover dados processados
    evbuffer_drain(input, len);
    free(data);
}

/**
 * @brief Callback chamado quando ocorre um evento na conexão
 * @param bev BufferEvent
 * @param events Eventos
 * @param user_data Dados do usuário (não usado)
 */
static void event_callback(struct bufferevent *bev, short events, void *user_data) {
    (void)user_data; // Suprimir warning
    
    tcp_connection_t *conn = find_connection_by_bev(bev);
    
    if (events & BEV_EVENT_CONNECTED) {
        LOG_INFO("TCP", "Conexão estabelecida");
        return;
    }
    
    if (events & (BEV_EVENT_ERROR | BEV_EVENT_EOF)) {
        if (events & BEV_EVENT_ERROR) {
            int err = EVUTIL_SOCKET_ERROR();
            LOG_ERROR("TCP", "Erro na conexão: %s", evutil_socket_error_to_string(err));
        } else {
            LOG_INFO("TCP", "Conexão fechada pelo peer");
        }
        
        remove_connection(bev);
        bufferevent_free(bev);
    }
}

/**
 * @brief Callback chamado quando nova conexão é aceita
 * @param listener Listener
 * @param fd File descriptor da nova conexão
 * @param address Endereço do cliente
 * @param socklen Tamanho do endereço
 * @param user_data Dados do usuário (não usado)
 */
static void accept_callback(struct evconnlistener *listener, evutil_socket_t fd,
                          struct sockaddr *address, int socklen, void *user_data) {
    (void)listener;    // Suprimir warning
    (void)socklen;     // Suprimir warning
    (void)user_data;   // Suprimir warning
    
    struct sockaddr_in *sin = (struct sockaddr_in*)address;
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin->sin_addr, addr_str, INET_ADDRSTRLEN);
    int port = ntohs(sin->sin_port);
    
    LOG_INFO("TCP", "Nova conexão aceita de %s:%d", addr_str, port);
    
    // Criar bufferevent para nova conexão
    struct bufferevent *bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        LOG_ERROR("TCP", "Erro ao criar bufferevent");
        close(fd);
        return;
    }
    
    // Configurar callbacks
    bufferevent_setcb(bev, read_callback, NULL, event_callback, NULL);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    
    // Adicionar à lista de conexões
    add_connection(bev, addr_str, port, false);
}

/**
 * @brief Callback chamado quando ocorre erro no listener
 * @param listener Listener
 * @param user_data Dados do usuário (não usado)
 */
static void accept_error_callback(struct evconnlistener *listener, void *user_data) {
    (void)user_data; // Suprimir warning
    
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR();
    LOG_ERROR("TCP", "Erro no listener: %s", evutil_socket_error_to_string(err));
    event_base_loopexit(base, NULL);
}

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

/**
 * @brief Inicializa o sistema de comunicação TCP
 * @param port Porta para escutar conexões (0 = não escutar)
 * @return 0 se sucesso, -1 se erro
 */
int tcp_init(int listen_port) {
    if (tcp_initialized) {
        LOG_WARN("TCP", "TCP já inicializado");
        return 0;
    }
    
    LOG_INFO("TCP", "Inicializando sistema TCP...");
    
    // Criar base de eventos
    base = event_base_new();
    if (!base) {
        LOG_ERROR("TCP", "Erro ao criar base de eventos");
        return -1;
    }
    
    // Criar listener se porta especificada
    if (listen_port > 0) {
        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_addr.s_addr = INADDR_ANY;
        sin.sin_port = htons(listen_port);
        
        listener = evconnlistener_new_bind(base, accept_callback, NULL,
                                         LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
                                         -1, (struct sockaddr*)&sin, sizeof(sin));
        
        if (!listener) {
            LOG_ERROR("TCP", "Erro ao criar listener na porta %d: %s", 
                     listen_port, strerror(errno));
            event_base_free(base);
            base = NULL;
            return -1;
        }
        
        evconnlistener_set_error_cb(listener, accept_error_callback);
        LOG_INFO("TCP", "Escutando na porta %d", listen_port);
    }
    
    // Zerar contadores
    connection_count = 0;
    memset(active_connections, 0, sizeof(active_connections));
    
    tcp_initialized = true;
    LOG_INFO("TCP", "Sistema TCP inicializado com sucesso");
    return 0;
}

/**
 * @brief Finaliza o sistema TCP
 */
void tcp_cleanup(void) {
    if (!tcp_initialized) return;
    
    LOG_INFO("TCP", "Finalizando sistema TCP...");
    
    // Fechar todas as conexões
    pthread_mutex_lock(&tcp_mutex);
    for (int i = 0; i < connection_count; i++) {
        if (active_connections[i].bev) {
            bufferevent_free(active_connections[i].bev);
        }
    }
    connection_count = 0;
    pthread_mutex_unlock(&tcp_mutex);
    
    // Liberar listener
    if (listener) {
        evconnlistener_free(listener);
        listener = NULL;
    }
    
    // Liberar base de eventos
    if (base) {
        event_base_free(base);
        base = NULL;
    }
    
    tcp_initialized = false;
    LOG_INFO("TCP", "Sistema TCP finalizado");
}

/**
 * @brief Conecta a um servidor remoto
 * @param address Endereço IP
 * @param port Porta
 * @return Conexão se sucesso, NULL se erro
 */
tcp_connection_t* tcp_connect(const char *address, int port) {
    if (!tcp_initialized || !address) {
        LOG_ERROR("TCP", "TCP não inicializado ou endereço inválido");
        return NULL;
    }
    
    LOG_INFO("TCP", "Conectando a %s:%d", address, port);
    
    // Criar socket
    struct bufferevent *bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        LOG_ERROR("TCP", "Erro ao criar bufferevent");
        return NULL;
    }
    
    // Configurar callbacks
    bufferevent_setcb(bev, read_callback, NULL, event_callback, NULL);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
    
    // Conectar
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    if (inet_pton(AF_INET, address, &sin.sin_addr) <= 0) {
        LOG_ERROR("TCP", "Endereço IP inválido: %s", address);
        bufferevent_free(bev);
        return NULL;
    }
    
    if (bufferevent_socket_connect(bev, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        LOG_ERROR("TCP", "Erro ao conectar: %s", strerror(errno));
        bufferevent_free(bev);
        return NULL;
    }
    
    // Adicionar à lista de conexões
    return add_connection(bev, address, port, true);
}

/**
 * @brief Envia uma mensagem para uma conexão
 * @param conn Conexão
 * @param message Mensagem
 * @return 0 se sucesso, -1 se erro
 */
int tcp_send_message(tcp_connection_t *conn, const tcp_message_t *message) {
    if (!conn || !message || !conn->bev) {
        LOG_ERROR("TCP", "Parâmetros inválidos para envio");
        return -1;
    }
    
    // Criar mensagem simples no formato: type=xxx,data=yyy,timestamp=zzz
    char msg_buffer[1024];
    const char *type_str;
    
    switch (message->type) {
        case TCP_MSG_PARKING_STATUS: type_str = "parking_status"; break;
        case TCP_MSG_VEHICLE_ENTRY: type_str = "vehicle_entry"; break;
        case TCP_MSG_VEHICLE_EXIT: type_str = "vehicle_exit"; break;
        case TCP_MSG_SYSTEM_STATUS: type_str = "system_status"; break;
        case TCP_MSG_EMERGENCY: type_str = "emergency"; break;
        default: type_str = "unknown"; break;
    }
    
    // Formatar mensagem
    snprintf(msg_buffer, sizeof(msg_buffer), "type=%s,timestamp=%ld,source=%s,data=%s",
             type_str, message->timestamp, message->source, message->data);
    
    size_t msg_len = strlen(msg_buffer);
    
    // Enviar dados (adicionar \n ao final)
    struct evbuffer *output = bufferevent_get_output(conn->bev);
    evbuffer_add(output, msg_buffer, msg_len);
    evbuffer_add(output, "\n", 1);
    
    // Atualizar estatísticas
    conn->last_activity = time(NULL);
    conn->bytes_sent += msg_len + 1;
    
    LOG_DEBUG("TCP", "Mensagem enviada para %s:%d: %s", conn->address, conn->port, msg_buffer);
    
    return 0;
}

/**
 * @brief Executa o loop de eventos (bloqueante)
 * @return 0 se saiu normalmente, -1 se erro
 */
int tcp_run_loop(void) {
    if (!tcp_initialized || !base) {
        LOG_ERROR("TCP", "TCP não inicializado");
        return -1;
    }
    
    LOG_INFO("TCP", "Iniciando loop de eventos TCP...");
    return event_base_dispatch(base);
}

/**
 * @brief Para o loop de eventos
 */
void tcp_stop_loop(void) {
    if (base) {
        event_base_loopexit(base, NULL);
        LOG_INFO("TCP", "Loop de eventos TCP parado");
    }
}

/**
 * @brief Define callback para mensagens recebidas
 * @param callback Função callback
 */
void tcp_set_message_callback(tcp_message_callback_t callback) {
    message_callback = callback;
}

/**
 * @brief Define callback para eventos de conexão
 * @param callback Função callback
 */
void tcp_set_connection_callback(tcp_connection_callback_t callback) {
    connection_callback = callback;
}

/**
 * @brief Obtém informações sobre conexões ativas
 * @param connections Array para armazenar conexões
 * @param max_connections Tamanho máximo do array
 * @return Número de conexões ativas
 */
int tcp_get_connections(tcp_connection_t *connections, int max_connections) {
    if (!connections || max_connections <= 0) return 0;
    
    pthread_mutex_lock(&tcp_mutex);
    int count = (connection_count < max_connections) ? connection_count : max_connections;
    memcpy(connections, active_connections, count * sizeof(tcp_connection_t));
    pthread_mutex_unlock(&tcp_mutex);
    
    return count;
}

/**
 * @brief Desconecta uma conexão específica
 * @param conn Conexão para desconectar
 */
void tcp_disconnect(tcp_connection_t *conn) {
    if (!conn || !conn->bev) return;
    
    LOG_INFO("TCP", "Desconectando %s:%d", conn->address, conn->port);
    remove_connection(conn->bev);
    bufferevent_free(conn->bev);
}