#ifndef LOG_H
#define LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

// Log levels
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARN = 2,
    LOG_ERROR = 3,
    LOG_FATAL = 4
} log_level_t;

// Log configuration
#define MAX_LOG_MESSAGE_SIZE 512
#define MAX_LOG_FILES 10
#define MAX_LOG_FILE_SIZE (1024 * 1024)  // 1MB per log file
#define LOG_BUFFER_SIZE 4096

// Log file management
typedef struct {
    char filename[64];
    int fd;
    size_t current_size;
    bool is_open;
    log_level_t min_level;
} log_file_t;

// Global logging state
typedef struct {
    log_file_t files[MAX_LOG_FILES];
    int num_files;
    char log_buffer[LOG_BUFFER_SIZE];
    int buffer_pos;
    bool initialized;
    log_level_t console_level;
} log_state_t;

// Initialize logging system
bool log_init(void);

// File operations
int log_open_file(const char *filename, log_level_t min_level);
void log_close_file(int log_id);
void log_close_all_files(void);

// Logging functions
void log_message(log_level_t level, const char *component, const char *fmt, ...);
void log_to_file(int log_id, log_level_t level, const char *component, const char *fmt, ...);

// Convenience macros
#define LOG_DEBUG_MSG(component, ...) log_message(LOG_DEBUG, component, __VA_ARGS__)
#define LOG_INFO_MSG(component, ...)  log_message(LOG_INFO, component, __VA_ARGS__)
#define LOG_WARN_MSG(component, ...)  log_message(LOG_WARN, component, __VA_ARGS__)
#define LOG_ERROR_MSG(component, ...) log_message(LOG_ERROR, component, __VA_ARGS__)
#define LOG_FATAL_MSG(component, ...) log_message(LOG_FATAL, component, __VA_ARGS__)

// System-specific logging
#define KLOG_DEBUG(...) log_message(LOG_DEBUG, "kernel", __VA_ARGS__)
#define KLOG_INFO(...)  log_message(LOG_INFO, "kernel", __VA_ARGS__)
#define KLOG_WARN(...)  log_message(LOG_WARN, "kernel", __VA_ARGS__)
#define KLOG_ERROR(...) log_message(LOG_ERROR, "kernel", __VA_ARGS__)

#define NETLOG_DEBUG(...) log_message(LOG_DEBUG, "network", __VA_ARGS__)
#define NETLOG_INFO(...)  log_message(LOG_INFO, "network", __VA_ARGS__)

#define IPCLOG_DEBUG(...) log_message(LOG_DEBUG, "ipc", __VA_ARGS__)
#define IPCLOG_INFO(...)  log_message(LOG_INFO, "ipc", __VA_ARGS__)

// Log management
void log_flush(void);
void log_rotate_if_needed(int log_id);
void log_set_console_level(log_level_t level);

// Utility functions
const char* log_level_string(log_level_t level);
void log_get_timestamp(char *buffer, size_t size);

#endif // LOG_H