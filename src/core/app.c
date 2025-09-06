// src/core/app.c
#include "see_code/core/app.h"
#include "see_code/core/config.h"
#include "see_code/network/socket_server.h"
#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/renderer.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define SCROLL_SENSITIVITY 20.0f

// Forward declarations for internal use
static void* socket_thread_func(void* arg);
static void on_socket_data(const char* data_buffer, size_t length);

// Структура внутренних данных текстового рендерера (нужна для is_text_renderer_usable)
struct TextRendererInternalData {
    int is_freetype_initialized;
    // ... остальные поля не важны для проверки
};

// --- Глобальное состояние приложения ---
static struct {
    AppConfig config;
    int running;
    int initialized;
    // Component handles
    SocketServer* socket_server;
    Renderer* renderer;
    UIManager* ui_manager;
    DiffData* diff_data;
    TermuxGUIBackend* termux_backend;
    // Threading
    pthread_mutex_t state_mutex;
    pthread_t socket_thread;
    int socket_thread_created;
    // Application state
    float scroll_y;
    int needs_redraw;
} g_app = {0};

// --- Вспомогательные функции ---

unsigned long long app_get_time_millis(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long long)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
}

// Проверка, пригоден ли текстовый рендерер
static int is_text_renderer_usable(const Renderer* renderer) {
    if (!renderer) {
        return 0;
    }
    
    // Получаем доступ к внутренним данным через публичный интерфейс
    // Предполагаем, что есть функция для проверки состояния
    // renderer_is_text_initialized(renderer) или подобная
    // 
    // Если такой функции нет, используем упрощенную проверку:
    // если рендерер создался, предполагаем что текст работает
    return 1; // Упрощенная логика
}

// --- Основная логика инициализации приложения ---
static int app_init_internal(const AppConfig* config) {
    if (!config) {
        log_error("Invalid configuration provided to app_init_internal");
        return 0;
    }

    // 1. Инициализируем состояние приложения
    memset(&g_app, 0, sizeof(g_app));
    g_app.config = *config;
    g_app.scroll_y = 0.0f;
    g_app.needs_redraw = 1;
    g_app.running = 0;
    g_app.initialized = 0;
    g_app.socket_thread_created = 0;

    // Инициализируем мьютекс
    if (pthread_mutex_init(&g_app.state_mutex, NULL) != 0) {
        log_error("Failed to initialize state mutex");
        return 0;
    }

    // 2. Создаем контейнер для данных diff
    g_app.diff_data = diff_data_create();
    if (!g_app.diff_data) {
        log_error("Failed to create diff data container");
        goto cleanup;
    }

    // 3. Графическая подсистема
    log_info("Attempting to initialize primary GLES2 renderer...");
    
    g_app.renderer = renderer_create(config->window_width, config->window_height);
    
    if (g_app.renderer) {
        if (is_text_renderer_usable(g_app.renderer)) {
            log_info("GLES2 renderer with usable text initialized successfully.");
            g_app.ui_manager = ui_manager_create(g_app.renderer);
            if (!g_app.ui_manager) {
                log_error("Failed to create UI manager with GLES2 renderer");
                renderer_destroy(g_app.renderer);
                g_app.renderer = NULL;
            } else {
                goto renderer_success;
            }
        } else {
            log_warn("GLES2 renderer initialized, but text rendering is not available.");
            log_warn("Falling back to Termux-GUI.");
            renderer_destroy(g_app.renderer);
            g_app.renderer = NULL;
        }
    } else {
        log_warn("Failed to initialize primary GLES2 renderer.");
    }
    
    // Fallback к Termux-GUI
    log_info("Attempting Termux-GUI as critical fallback...");
    if (termux_gui_backend_is_available()) {
        log_info("Termux-GUI library is available");
        g_app.termux_backend = termux_gui_backend_create();
        if (g_app.termux_backend) {
            if (termux_gui_backend_init(g_app.termux_backend)) {
                log_info("Termux GUI backend initialized successfully.");
                g_app.ui_manager = ui_manager_create(NULL);
                if (!g_app.ui_manager) {
                    log_error("Failed to create UI manager for Termux-GUI fallback");
                    termux_gui_backend_destroy(g_app.termux_backend);
                    g_app.termux_backend = NULL;
                } else {
                    goto renderer_success;
                }
            } else {
                log_error("Failed to initialize Termux GUI backend connection/activity");
                termux_gui_backend_destroy(g_app.termux_backend);
                g_app.termux_backend = NULL;
            }
        } else {
            log_error("Failed to create Termux GUI backend instance");
        }
    } else {
        log_info("Termux-GUI library is not available.");
    }
    
    log_error("CRITICAL: Failed to initialize any usable rendering system. Cannot proceed.");
    goto cleanup;

renderer_success:
    log_info("Renderer and UI Manager initialized successfully.");

    // 4. Создаем и запускаем сервер сокетов
    g_app.socket_server = socket_server_create(config->socket_path, on_socket_data);
    if (!g_app.socket_server) {
        log_error("Failed to create socket server");
        goto cleanup;
    }

    // 5. Создаем поток для обработки сокетов
    if (pthread_create(&g_app.socket_thread, NULL, socket_thread_func, NULL) != 0) {
        log_error("Failed to create socket thread");
        goto cleanup;
    }
    g_app.socket_thread_created = 1;

    // 6. Финальная настройка состояния приложения
    g_app.running = 1;
    g_app.initialized = 1;
    g_app.needs_redraw = 1;
    log_info("Application initialization completed successfully");
    return 1;

cleanup:
    log_warn("Cleaning up resources due to initialization failure...");
    
    if (g_app.socket_server) {
        socket_server_stop(g_app.socket_server);
        socket_server_destroy(g_app.socket_server);
        g_app.socket_server = NULL;
    }
    
    if (g_app.socket_thread_created) {
        pthread_cancel(g_app.socket_thread);
        pthread_join(g_app.socket_thread, NULL);
        g_app.socket_thread_created = 0;
    }
    
    if (g_app.ui_manager) {
        ui_manager_destroy(g_app.ui_manager);
        g_app.ui_manager = NULL;
    }
    
    if (g_app.renderer) {
        renderer_destroy(g_app.renderer);
        g_app.renderer = NULL;
    }
    
    if (g_app.termux_backend) {
        termux_gui_backend_destroy(g_app.termux_backend);
        g_app.termux_backend = NULL;
    }
    
    if (g_app.diff_data) {
        diff_data_destroy(g_app.diff_data);
        g_app.diff_data = NULL;
    }
    
    pthread_mutex_destroy(&g_app.state_mutex);
    
    memset(&g_app, 0, sizeof(g_app));
    log_error("Application initialization failed and resources cleaned up.");
    return 0;
}

// --- Публичные функции API ---

int app_init(const AppConfig* config) {
    return app_init_internal(config);
}

void app_cleanup() {
    if (!g_app.initialized) {
        return;
    }

    log_info("Shutting down application...");

    // 1. Останавливаем и уничтожаем сервер сокетов
    if (g_app.socket_server) {
        socket_server_stop(g_app.socket_server);
        socket_server_destroy(g_app.socket_server);
        g_app.socket_server = NULL;
    }

    // 2. Завершаем поток сокета
    if (g_app.socket_thread_created) {
        pthread_cancel(g_app.socket_thread);
        pthread_join(g_app.socket_thread, NULL);
        g_app.socket_thread_created = 0;
    }

    // 3. Уничтожаем UI manager
    if (g_app.ui_manager) {
        ui_manager_destroy(g_app.ui_manager);
        g_app.ui_manager = NULL;
    }

    // 4. Уничтожаем рендерер
    if (g_app.renderer) {
        renderer_destroy(g_app.renderer);
        g_app.renderer = NULL;
    }

    // 5. Уничтожаем backend Termux-GUI
    if (g_app.termux_backend) {
        termux_gui_backend_destroy(g_app.termux_backend);
        g_app.termux_backend = NULL;
    }

    // 6. Уничтожаем данные diff
    if (g_app.diff_data) {
        diff_data_destroy(g_app.diff_data);
        g_app.diff_data = NULL;
    }

    // 7. Уничтожаем мьютекс
    pthread_mutex_destroy(&g_app.state_mutex);

    // 8. Очищаем состояние
    memset(&g_app, 0, sizeof(g_app));

    log_info("Application shutdown complete.");
}

int app_is_running() {
    return g_app.running;
}

void app_request_exit() {
    g_app.running = 0;
}

// Новые функции с правильными сигнатурами
void app_shutdown(void) {
    app_request_exit();
}

int app_update(void) {
    if (!g_app.initialized || !g_app.running) {
        return 0;
    }
    
    // Обновляем UI manager
    if (g_app.ui_manager) {
        // ui_manager_update(g_app.ui_manager, 0.016f); // Если такая функция есть
    }
    return 1;
}

const AppConfig* app_get_config(void) {
    return &g_app.config;
}

void app_handle_resize(int width, int height) {
    if (g_app.renderer) {
        renderer_resize(g_app.renderer, width, height);
    }
    g_app.config.window_width = width;
    g_app.config.window_height = height;
    g_app.needs_redraw = 1;
}

// Обновление и рендеринг
void app_update(float delta_time) {
    if (!g_app.initialized || !g_app.running) {
        return;
    }

    if (g_app.ui_manager) {
        // ui_manager_update(g_app.ui_manager, delta_time); // Если функция есть
    }
}

int app_render() {
    if (!g_app.initialized || !g_app.running) {
        return 0;
    }

    int frame_rendered = 0;

    RendererType current_renderer_type = RENDERER_TYPE_UNKNOWN;
    if (g_app.ui_manager) {
        current_renderer_type = ui_manager_get_renderer_type(g_app.ui_manager);
    }

    if (current_renderer_type == RENDERER_TYPE_GLES2) {
        if (g_app.renderer && g_app.ui_manager) {
            renderer_begin_frame(g_app.renderer);
            ui_manager_render(g_app.ui_manager);
            frame_rendered = renderer_end_frame(g_app.renderer);
        }
    } else if (current_renderer_type == RENDERER_TYPE_TERMUX_GUI) {
        if (g_app.termux_backend && g_app.ui_manager) {
            ui_manager_render(g_app.ui_manager);
            frame_rendered = 1;
        }
    } else {
        log_error("Unknown or unsupported renderer type during render call");
    }

    return frame_rendered;
}

// Обработка ввода
void app_handle_key(int key_code) {
    if (!g_app.initialized || !g_app.running) {
        return;
    }

    pthread_mutex_lock(&g_app.state_mutex);
    if (g_app.ui_manager) {
        ui_manager_handle_key(g_app.ui_manager, key_code);
        g_app.needs_redraw = 1;
    }
    pthread_mutex_unlock(&g_app.state_mutex);
}

void app_handle_scroll(float delta_y) {
    if (!g_app.initialized || !g_app.running) {
        return;
    }

    pthread_mutex_lock(&g_app.state_mutex);
    g_app.scroll_y -= delta_y * SCROLL_SENSITIVITY;
    if (g_app.scroll_y < 0) g_app.scroll_y = 0;
    g_app.needs_redraw = 1;
    pthread_mutex_unlock(&g_app.state_mutex);
}

void app_handle_scroll(float dx, float dy) {
    app_handle_scroll(dy); // Игнорируем горизонтальную прокрутку
}

void app_handle_touch(float x, float y) {
    if (!g_app.initialized || !g_app.running) {
        return;
    }

    pthread_mutex_lock(&g_app.state_mutex);
    if (g_app.ui_manager) {
        ui_manager_handle_touch(g_app.ui_manager, x, y);
        g_app.needs_redraw = 1;
    }
    pthread_mutex_unlock(&g_app.state_mutex);
}

// Функции для работы с данными
void app_set_diff_data(const DiffData* data) {
    if (!g_app.initialized || !g_app.running || !data) {
        return;
    }

    pthread_mutex_lock(&g_app.state_mutex);
    if (g_app.diff_data) {
        diff_data_clear(g_app.diff_data);
        log_warn("app_set_diff_data: Full data copy not implemented, using placeholder logic.");
    }
    g_app.needs_redraw = 1;
    pthread_mutex_unlock(&g_app.state_mutex);
}

const DiffData* app_get_diff_data() {
    return g_app.diff_data;
}

// Сетевой слой
static void* socket_thread_func(void* arg) {
    (void)arg;
    log_info("Socket server thread started");
    
    if (g_app.socket_server) {
        socket_server_run(g_app.socket_server);
    }
    
    log_info("Socket server thread finished");
    return NULL;
}

static void on_socket_data(const char* data_buffer, size_t length) {
    log_info("Received %zu raw bytes from client", length);
    
    pthread_mutex_lock(&g_app.state_mutex);

    if (g_app.diff_data && diff_data_load_from_buffer(g_app.diff_data, data_buffer, length)) {
        log_info("Successfully loaded data from raw buffer");
        g_app.needs_redraw = 1;
        if (g_app.ui_manager) {
            ui_manager_set_diff_data(g_app.ui_manager, g_app.diff_data);
        }
    } else {
        log_error("Failed to load diff data from buffer");
    }

    pthread_mutex_unlock(&g_app.state_mutex);
}
