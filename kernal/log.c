#include "log.h"
#include "printk.h"
#include "timer.h"
#include <stddef.h>

extern int fs_open(const char *path, int flags);
extern int fs_write(int fd, const void *buf, size_t count);
extern void fs_close(int fd);
extern int fs_create_directory(const char *path);

// Global logging state
static log_state_t g_log_state = {0};

// Helper function to copy strings safely
static void safe_strcpy(char *dest, const char *src, size_t max_len) {
    size_t i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static void safe_strcat(char *dest, const char *src, size_t max_len) {
    size_t dest_len = 0;
    
    // Find end of dest string
    while (dest_len < max_len - 1 && dest[dest_len] != '\0') {
        dest_len++;
    }
    
    // Append src to dest
    size_t i = 0;
    while (dest_len < max_len - 1 && src[i] != '\0') {
        dest[dest_len] = src[i];
        dest_len++;
        i++;
    }
    dest[dest_len] = '\0';
}

// Simple integer to string conversion
static void int_to_str(int value, char *str) {
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int is_negative = 0;
    if (value < 0) {
        is_negative = 1;
        value = -value;
    }
    
    char temp[16];
    int pos = 0;
    
    while (value > 0) {
        temp[pos++] = '0' + (value % 10);
        value /= 10;
    }
    
    int str_pos = 0;
    if (is_negative) {
        str[str_pos++] = '-';
    }
    
    while (pos > 0) {
        str[str_pos++] = temp[--pos];
    }
    str[str_pos] = '\0';
}

bool log_init(void) {
    if (g_log_state.initialized) {
        return true;
    }
    
    // Initialize the logging state
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        g_log_state.files[i].is_open = false;
        g_log_state.files[i].fd = -1;
        g_log_state.files[i].current_size = 0;
        g_log_state.files[i].min_level = LOG_INFO;
    }
    
    g_log_state.num_files = 0;
    g_log_state.buffer_pos = 0;
    g_log_state.console_level = LOG_INFO;
    
    g_log_state.initialized = true;
    
    printk("[log] Logging system initialized\n");
    return true;
}

const char* log_level_string(log_level_t level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        case LOG_FATAL: return "FATAL";
        default:        return "UNKN ";
    }
}

void log_get_timestamp(char *buffer, size_t size) {
    // Get system uptime in ms
    extern uint64_t get_uptime_ms(void);
    uint64_t uptime = get_uptime_ms();
    
    // Convert to seconds and ms
    uint32_t seconds = uptime / 1000;
    uint32_t ms = uptime % 1000;
    
    // Format as [ssss.mmm]
    char sec_str[16], ms_str[8];
    int_to_str(seconds, sec_str);
    int_to_str(ms, ms_str);
    
    // Build timestamp string
    buffer[0] = '[';
    buffer[1] = '\0';
    safe_strcat(buffer, sec_str, size);
    safe_strcat(buffer, ".", size);
    
    // Pad milliseconds to 3 digits
    if (ms < 100) safe_strcat(buffer, "0", size);
    if (ms < 10) safe_strcat(buffer, "0", size);
    safe_strcat(buffer, ms_str, size);
    safe_strcat(buffer, "]", size);
}

int log_open_file(const char *filename, log_level_t min_level) {
    if (!g_log_state.initialized) {
        log_init();
    }
    
    if (g_log_state.num_files >= MAX_LOG_FILES) {
        printk("[log] ERROR: Maximum number of log files reached\n");
        return -1;
    }
    
    // Create full path with logs/ prefix
    char full_path[128] = "logs/";
    safe_strcat(full_path, filename, sizeof(full_path));
    
    int log_id = -1;
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        if (!g_log_state.files[i].is_open) {
            log_id = i;
            break;
        }
    }
    
    if (log_id == -1) {
        printk("[log] ERROR: No available log file slots\n");
        return -1;
    }
    
    // Initialize log file structure
    log_file_t *log_file = &g_log_state.files[log_id];
    safe_strcpy(log_file->filename, full_path, sizeof(log_file->filename));
    log_file->min_level = min_level;
    log_file->current_size = 0;
    log_file->is_open = true;
    
    log_file->fd = log_id + 100;  // Fake file descriptor
    
    g_log_state.num_files++;
    
    printk("[log] Opened log file: %s (ID: %d)\n", full_path, log_id);
    return log_id;
}

void log_close_file(int log_id) {
    if (log_id < 0 || log_id >= MAX_LOG_FILES) {
        return;
    }
    
    log_file_t *log_file = &g_log_state.files[log_id];
    if (log_file->is_open) {
        // close the actual file
        // fs_close(log_file->fd);
        
        log_file->is_open = false;
        log_file->fd = -1;
        g_log_state.num_files--;
        
        printk("[log] Closed log file: %s\n", log_file->filename);
    }
}

void log_close_all_files(void) {
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        if (g_log_state.files[i].is_open) {
            log_close_file(i);
        }
    }
}

static void format_log_message(char *buffer, size_t size, log_level_t level, 
                              const char *component, const char *message) {
    char timestamp[32];
    log_get_timestamp(timestamp, sizeof(timestamp));
    
    // Format: [timestamp] LEVEL [component] message
    buffer[0] = '\0';
    safe_strcat(buffer, timestamp, size);
    safe_strcat(buffer, " ", size);
    safe_strcat(buffer, log_level_string(level), size);
    safe_strcat(buffer, " [", size);
    safe_strcat(buffer, component, size);
    safe_strcat(buffer, "] ", size);
    safe_strcat(buffer, message, size);
    safe_strcat(buffer, "\n", size);
}

void log_message(log_level_t level, const char *component, const char *fmt, ...) {
    if (!g_log_state.initialized) {
        log_init();
    }
    
    // Format the message
    char message[MAX_LOG_MESSAGE_SIZE];
    va_list args;
    va_start(args, fmt);
    
    safe_strcpy(message, fmt, sizeof(message));
    va_end(args);
    
    // Format complete log entry
    char log_entry[MAX_LOG_MESSAGE_SIZE + 128];
    format_log_message(log_entry, sizeof(log_entry), level, component, message);
    
    // Output to console if level is high enough
    if (level >= g_log_state.console_level) {
        printk("%s", log_entry);
    }
    
    // Write to all appropriate log files
    for (int i = 0; i < MAX_LOG_FILES; i++) {
        log_file_t *log_file = &g_log_state.files[i];
        if (log_file->is_open && level >= log_file->min_level) {
            size_t entry_len = 0;
            while (log_entry[entry_len] != '\0' && entry_len < sizeof(log_entry)) {
                entry_len++;
            }
            
            log_file->current_size += entry_len;
            
            // Check if rotation is needed
            log_rotate_if_needed(i);
        }
    }
}

void log_to_file(int log_id, log_level_t level, const char *component, const char *fmt, ...) {
    if (log_id < 0 || log_id >= MAX_LOG_FILES) {
        return;
    }
    
    log_file_t *log_file = &g_log_state.files[log_id];
    if (!log_file->is_open || level < log_file->min_level) {
        return;
    }
    
    // Format the message
    char message[MAX_LOG_MESSAGE_SIZE];
    va_list args;
    va_start(args, fmt);
    safe_strcpy(message, fmt, sizeof(message));
    va_end(args);
    
    // Format complete log entry
    char log_entry[MAX_LOG_MESSAGE_SIZE + 128];
    format_log_message(log_entry, sizeof(log_entry), level, component, message);
    
    // Write to file (simulated)
    size_t entry_len = 0;
    while (log_entry[entry_len] != '\0' && entry_len < sizeof(log_entry)) {
        entry_len++;
    }
    
    log_file->current_size += entry_len;
    log_rotate_if_needed(log_id);
}

void log_rotate_if_needed(int log_id) {
    if (log_id < 0 || log_id >= MAX_LOG_FILES) {
        return;
    }
    
    log_file_t *log_file = &g_log_state.files[log_id];
    if (!log_file->is_open) {
        return;
    }
    
    if (log_file->current_size >= MAX_LOG_FILE_SIZE) {
        printk("[log] Log file %s would be rotated (size: %d bytes)\n", 
               log_file->filename, (int)log_file->current_size);
        log_file->current_size = 0;
    }
}

void log_set_console_level(log_level_t level) {
    g_log_state.console_level = level;
    printk("[log] Console log level set to: %s\n", log_level_string(level));
}

void log_flush(void) {
    // In a real implementation, this would flush all open log files
    printk("[log] Flushing all log files\n");
}