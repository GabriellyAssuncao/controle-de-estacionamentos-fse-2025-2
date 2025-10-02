/**
 * @file system_logger.c
 * @brief Sistema de logging para debug e monitoramento
 */

#include "system_logger.h"
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/time.h>

// =============================================================================
// VARIÁVEIS GLOBAIS
// =============================================================================

static FILE* log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;
static char log_directory[256] = {0};
static log_level_t current_log_level = DEFAULT_LOG_LEVEL;

// Nomes dos níveis de log
static const char* level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

// Cores ANSI para terminal
static const char* level_colors[] = {
    "\x1b[36m", // Cyan para DEBUG
    "\x1b[32m", // Verde para INFO
    "\x1b[33m", // Amarelo para WARN
    "\x1b[31m", // Vermelho para ERROR
    "\x1b[35m"  // Magenta para FATAL
};

// =============================================================================
// FUNÇÕES INTERNAS
// =============================================================================

/**
 * @brief Cria o diretório de log se não existir
 */
static int create_log_directory(const char* dir) {
    struct stat st = {0};
    
    if (stat(dir, &st) == -1) {
        if (mkdir(dir, 0755) == -1) {
            fprintf(stderr, "Erro ao criar diretório de log %s: %s\n", 
                    dir, strerror(errno));
            return -1;
        }
    }
    
    return 0;
}

/**
 * @brief Obtém timestamp formatado
 */
static void get_timestamp(char* buffer, size_t size) {
    struct timeval tv;
    struct tm* tm_info;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    snprintf(buffer, size, "%04d-%02d-%02d %02d:%02d:%02d.%03ld",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             tv.tv_usec / 1000);
}

/**
 * @brief Rotaciona arquivo de log se necessário
 */
static void rotate_log_file_if_needed(void) {
    if (!log_file) return;
    
    long file_size = ftell(log_file);
    if (file_size > (LOG_FILE_MAX_SIZE_MB * 1024 * 1024)) {
        fclose(log_file);
        
        // Renomeia arquivo atual
        char old_path[512], new_path[512];
        snprintf(old_path, sizeof(old_path), "%s/parking_system.log", log_directory);
        snprintf(new_path, sizeof(new_path), "%s/parking_system.log.1", log_directory);
        rename(old_path, new_path);
        
        // Reabre arquivo
        log_file = fopen(old_path, "a");
        if (!log_file) {
            fprintf(stderr, "Erro ao reabrir arquivo de log\n");
        }
    }
}

// =============================================================================
// FUNÇÕES PÚBLICAS
// =============================================================================

int logger_init(const char* log_dir) {
    pthread_mutex_lock(&log_mutex);
    
    // Cria diretório se necessário
    if (create_log_directory(log_dir) != 0) {
        pthread_mutex_unlock(&log_mutex);
        return -1;
    }
    
    strncpy(log_directory, log_dir, sizeof(log_directory) - 1);
    
    // Abre arquivo de log
    char log_path[512];
    snprintf(log_path, sizeof(log_path), "%s/parking_system.log", log_dir);
    
    log_file = fopen(log_path, "a");
    if (!log_file) {
        fprintf(stderr, "Erro ao abrir arquivo de log %s: %s\n", 
                log_path, strerror(errno));
        pthread_mutex_unlock(&log_mutex);
        return -1;
    }
    
    pthread_mutex_unlock(&log_mutex);
    
    // Log inicial
    logger_log(LOG_LEVEL_INFO, "LOGGER", "Sistema de logging inicializado - arquivo: %s", log_path);
    
    return 0;
}

void logger_cleanup(void) {
    pthread_mutex_lock(&log_mutex);
    
    if (log_file) {
        logger_log(LOG_LEVEL_INFO, "LOGGER", "Finalizando sistema de logging");
        fclose(log_file);
        log_file = NULL;
    }
    
    pthread_mutex_unlock(&log_mutex);
}

void logger_log(log_level_t level, const char* module, const char* format, ...) {
    if (level < current_log_level) {
        return; // Nível muito baixo, ignora
    }
    
    pthread_mutex_lock(&log_mutex);
    
    char timestamp[32];
    get_timestamp(timestamp, sizeof(timestamp));
    
    // Formata mensagem
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // Log para arquivo
    if (log_file) {
        fprintf(log_file, "[%s] %s [%s] %s\n", 
                timestamp, level_names[level], module, message);
        fflush(log_file);
        rotate_log_file_if_needed();
    }
    
    // Log para console (sempre, independente do arquivo)
    printf("%s[%s] %s [%s] %s\x1b[0m\n", 
           level_colors[level], timestamp, level_names[level], module, message);
    fflush(stdout);
    
    pthread_mutex_unlock(&log_mutex);
}

void logger_set_level(log_level_t level) {
    current_log_level = level;
    logger_log(LOG_LEVEL_INFO, "LOGGER", "Nível de log alterado para: %s", level_names[level]);
}

log_level_t logger_get_level(void) {
    return current_log_level;
}