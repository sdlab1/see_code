// src/core/config.h
#ifndef SEE_CODE_CONFIG_H
#define SEE_CODE_CONFIG_H

// --- Paths ---
#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/see_code_socket"
#define FREETYPE_FONT_PATH "/system/fonts/Roboto-Regular.ttf"
#define TRUETYPE_FONT_PATH "/system/fonts/DroidSansMono.ttf"
#define FALLBACK_FONT_PATH "/data/data/com.termux/files/usr/share/fonts/liberation/LiberationMono-Regular.ttf"

// --- Sizes ---
#define WINDOW_WIDTH_DEFAULT 1080
#define WINDOW_HEIGHT_DEFAULT 1920
#define LINE_HEIGHT 20.0f
#define HUNK_HEADER_HEIGHT 25.0f
#define FILE_HEADER_HEIGHT 30.0f
#define MARGIN 10.0f
#define HUNK_PADDING 5.0f
#define SCROLL_SENSITIVITY 10.0f

// --- Colors (0xAARRGGBB) ---
#define COLOR_BACKGROUND 0xFF111111
#define COLOR_FILE_HEADER 0xFF4444FF
#define COLOR_HUNK_HEADER 0xFF00AA00
#define COLOR_ADD_LINE 0xFF00AA00
#define COLOR_DEL_LINE 0xFFAA0000
#define COLOR_CONTEXT_LINE 0xFF888888

// --- Font Sizes ---
#define FONT_SIZE_DEFAULT 14  // Исправлено: добавлено определение
#define ATLAS_WIDTH_DEFAULT 1024  // Исправлено: добавлено определение
#define ATLAS_HEIGHT_DEFAULT 1024  // Исправлено: добавлено определение

// --- Max Message Size ---
#define MAX_MESSAGE_SIZE (50 * 1024 * 1024) // 50MB

typedef struct {
    const char* socket_path;
    int window_width;
    int window_height;
    int landscape_mode;
    int verbose;
    int debug;
} AppConfig;

#define LOG_FILE_PATH "/data/data/com.termux/files/usr/tmp/see_code.log"
#define LOG_BUFFER_SIZE 1024

#endif // SEE_CODE_CONFIG_H
