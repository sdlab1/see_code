#ifndef SEE_CODE_CONFIG_H
#define SEE_CODE_CONFIG_H

// Application version and build information
#define APP_NAME "see_code"
#define APP_VERSION "1.0.0"
#define APP_DESCRIPTION "Interactive Git Diff Viewer for Termux"

// Socket configuration
#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/see_code_socket"
#define MAX_MESSAGE_SIZE (50 * 1024 * 1024)  // 50MB max message size
#define SOCKET_BUFFER_SIZE 8192

// UI Configuration
#define DEFAULT_WINDOW_WIDTH 1080
#define DEFAULT_WINDOW_HEIGHT 2400
#define LINE_HEIGHT 36.0f
#define FONT_SIZE 14
#define HEADER_FONT_SIZE 16
#define MARGIN 10.0f
#define HUNK_PADDING 20.0f

// Memory limits
#define MAX_FILES 256
#define MAX_HUNKS_PER_FILE 1024
#define MAX_LINES_PER_HUNK 2048
#define INITIAL_BUFFER_SIZE 16384

// Colors (RGBA format)
#define COLOR_BACKGROUND    0xFF1E1E1E
#define COLOR_FILE_HEADER   0xFF2D2D30
#define COLOR_HUNK_HEADER   0xFF404040
#define COLOR_ADD_LINE      0xFF00AA00
#define COLOR_DEL_LINE      0xFFAA0000
#define COLOR_CONTEXT_LINE  0xFFCCCCCC
#define COLOR_TEXT_PRIMARY  0xFFFFFFFF
#define COLOR_TEXT_SECONDARY 0xFFAAAAAA

// GLES2 Configuration
#define MAX_VERTICES 65536
#define MAX_INDICES 98304  // 1.5x vertices for efficiency
#define ATLAS_WIDTH 1024
#define ATLAS_HEIGHT 1024

// Font paths (FreeType fallback to TrueType)
#define FREETYPE_FONT_PATH "/system/fonts/Roboto-Regular.ttf"
#define TRUETYPE_FONT_PATH "/system/fonts/DroidSansMono.ttf"
#define FALLBACK_FONT_PATH "/data/data/com.termux/files/usr/share/fonts/liberation/LiberationMono-Regular.ttf"

// Logging configuration
#define LOG_BUFFER_SIZE 1024
#define LOG_FILE_PATH "/data/data/com.termux/files/usr/tmp/see_code.log"

// Performance tuning
#define TARGET_FPS 60
#define FRAME_TIME_MS (1000 / TARGET_FPS)
#define SCROLL_SENSITIVITY 10.0f
#define TOUCH_THRESHOLD 5.0f

// Application configuration structure
typedef struct {
    const char* socket_path;
    int window_width;
    int window_height;
    int landscape_mode;
    int verbose;
    int debug;
    const char* font_path;
    int font_size;
    int target_fps;
} AppConfig;

#endif // SEE_CODE_CONFIG_H
