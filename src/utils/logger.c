#include "see_code/utils/logger.h"
#include "see_code/core/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static FILE* g_log_file = NULL;
static LogLevel g_log_level = LOG_LEVEL_INFO;
static int g_verbose = 0;
static int g_debug = 0;

void logger_init(int verbose, int debug) {
    g_verbose = verbose;
    g_debug = debug;
    
    if (g_debug) {
        g_log_level = LOG_LEVEL_DEBUG;
    } else if (g_verbose) {
        g_log_level = LOG_LEVEL_INFO;
    } else {
        g_log_level = LOG_LEVEL_WARN;
    }
    
    // Open log file
    g_log_file = fopen(LOG_FILE_PATH, "a");
    if (!g_log_file) {
        fprintf(stderr, "Failed to open log file: %s\n", LOG_FILE_PATH);
    }
}

void logger_cleanup(void) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = NULL;
    }
}

static void log_message(LogLevel level, const char* format, va_list args) {
    if (level < g_log_level) {
        return;
    }
    
    // Get current time
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Determine log level string
    const char* level_str = "UNKNOWN";
    switch (level) {
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO";  break;
        case LOG_LEVEL_WARN:  level_str = "WARN";  break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
    }
    
    // Format message
    char message[LOG_BUFFER_SIZE];
    vsnprintf(message, sizeof(message), format, args);
    
    // Print to stderr
    fprintf(stderr, "[%s] [%s] %s\n", timestamp, level_str, message);
    
    // Write to log file if available
    if (g_log_file) {
        fprintf(g_log_file, "[%s] [%s] %s\n", timestamp, level_str, message);
        fflush(g_log_file);
    }
}

void log_debug(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_DEBUG, format, args);
    va_end(args);
}

void log_info(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_INFO, format, args);
    va_end(args);
}

void log_warn(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_WARN, format, args);
    va_end(args);
}

void log_error(const char* format, ...) {
    va_list args;
    va_start(args, format);
    log_message(LOG_LEVEL_ERROR, format, args);
    va_end(args);
}

void logger_set_level(LogLevel level) {
    g_log_level = level;
}
