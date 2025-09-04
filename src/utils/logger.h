#ifndef SEE_CODE_LOGGER_H
#define SEE_CODE_LOGGER_H

#include <stdio.h>
#include <stdarg.h>

// Logging levels
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_ERROR = 3
} LogLevel;

// Logger initialization and cleanup
void logger_init(int verbose, int debug);
void logger_cleanup(void);

// Logging functions
void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_warn(const char* format, ...);
void log_error(const char* format, ...);

// Set log level
void logger_set_level(LogLevel level);

#endif // SEE_CODE_LOGGER_H
