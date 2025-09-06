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
#define FONT_SIZE_DEFAULT 14
#define ATLAS_WIDTH_DEFAULT 1024
#define ATLAS_HEIGHT_DEFAULT 1024

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

// --- UI Widget Configuration ---
// Текстовое поле ввода
#define INPUT_FIELD_HEIGHT 60.0f
#define INPUT_FIELD_PLACEHOLDER_TEXT "type here"
#define INPUT_FIELD_PLACEHOLDER_COLOR 0xFF888888 // Серый
#define INPUT_FIELD_TEXT_COLOR 0xFF000000       // Черный
#define INPUT_FIELD_BACKGROUND_COLOR 0xFFFFFFFF // Белый
#define INPUT_FIELD_BORDER_COLOR 0xFF888888     // Серый
#define INPUT_FIELD_CURSOR_WIDTH 2.0f
#define INPUT_FIELD_CURSOR_COLOR 0xFF000000     // Черный
#define INPUT_FIELD_CURSOR_BLINK_INTERVAL_MS 500 // Миллисекунды

// Кнопка меню "..."
#define MENU_BUTTON_SIZE 40.0f
#define MENU_BUTTON_LABEL "..."
#define MENU_BUTTON_TEXT_COLOR 0xFF000000       // Черный
#define MENU_BUTTON_BACKGROUND_COLOR_DEFAULT 0xFFDDDDDD // Светло-серый
#define MENU_BUTTON_BACKGROUND_COLOR_HOVER 0xFFEEEEEE   // Еще светлее
#define MENU_BUTTON_BACKGROUND_COLOR_PRESSED 0xFFAAAAAA // Темнее
#define MENU_BUTTON_BORDER_COLOR 0xFF888888             // Серый для рамки

// UI Margins
#define UI_MARGIN 10.0f

// Определения дефолтных размеров окна
#define DEFAULT_WINDOW_WIDTH WINDOW_WIDTH_DEFAULT
#define DEFAULT_WINDOW_HEIGHT WINDOW_HEIGHT_DEFAULT

#endif // SEE_CODE_CONFIG_H
