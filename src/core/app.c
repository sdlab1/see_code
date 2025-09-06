// src/core/app.c
#include "see_code/core/app.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/ui_manager.h"
#include "see_code/network/socket_server.h"
#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"
#include "see_code/gui/termux_gui_backend.h" // Для fallback
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AppConfig config;
    int running;
    int initialized;
    // Component handles
    SocketServer* socket_server;
    Renderer* renderer;
    UIManager* ui_manager;
    DiffData* diff_data;
    TermuxGUIBackend* termux_backend; // Backend для fallback
    // Threading
    pthread_mutex_t state_mutex;
    pthread_t socket_thread;
    // Application state
    float scroll_y;
    int needs_redraw;
} AppState;

static AppState g_app = {0};

// Socket callback - called when raw data is received
static void on_socket_data(const char* data_buffer, size_t length) {
    log_info("Received %zu raw bytes from Neovim", length);
    pthread_mutex_lock(&g_app.state_mutex);

    // Load data from the raw buffer using the new parser
    if (diff_data_load_from_buffer(g_app.diff_data, data_buffer, length)) {
        log_info("Successfully loaded data from raw buffer");
        g_app.needs_redraw = 1;
        // Update UI with new data
        ui_manager_set_diff_data(g_app.ui_manager, g_app.diff_data);
    } else {
        log_error("Failed to load data from raw buffer");
    }

    pthread_mutex_unlock(&g_app.state_mutex);
}

int app_init(const AppConfig* config) {
    if (!config) {
        log_error("Null configuration provided");
        return 0;
    }
    if (g_app.initialized) {
        log_warn("Application already initialized");
        return 1;
    }

    // Copy configuration
    memcpy(&g_app.config, config, sizeof(AppConfig));

    // Initialize threading primitives
    if (pthread_mutex_init(&g_app.state_mutex, NULL) != 0) {
        log_error("Failed to initialize state mutex");
        return 0;
    }

    log_info("Initializing application components...");

    // Initialize diff data storage
    g_app.diff_data = diff_data_create();
    if (!g_app.diff_data) {
        log_error("Failed to initialize diff data storage");
        goto cleanup;
    }

    // --- ЛОГИКА ИНИЦИАЛИЗАЦИИ РЕНДЕРЕРА С FALLBACK ---
    // 1. Попробуем инициализировать основной GLES2 рендерер
    g_app.renderer = renderer_create(config->window_width, config->window_height);
    if (!g_app.renderer) {
        log_warn("Failed to initialize GLES2 renderer");
        // 2. GLES2 не удался, проверим и подготовим Termux-GUI как fallback
        if (termux_gui_backend_is_available()) {
            log_info("Termux-GUI library is available");
            g_app.termux_backend = termux_gui_backend_create();
            if (g_app.termux_backend) {
                if (termux_gui_backend_init(g_app.termux_backend)) {
                    log_info("Termux GUI backend initialized successfully for potential fallback");
                } else {
                    log_error("Failed to initialize Termux GUI backend connection/activity");
                    termux_gui_backend_destroy(g_app.termux_backend);
                    g_app.termux_backend = NULL;
                }
            } else {
                log_error("Failed to create Termux GUI backend instance");
            }
        } else {
            log_info("Termux-GUI library is not available");
        }

        // 3. Создаем UI manager с NULL renderer, он должен переключиться на fallback
        g_app.ui_manager = ui_manager_create(NULL);
        if (!g_app.ui_manager) {
            log_error("Failed to initialize UI manager for fallback mode");
            goto cleanup;
        }
        // Явно указываем использовать fallback
        ui_manager_set_renderer_type(g_app.ui_manager, RENDERER_TYPE_TERMUX_GUI);
        log_info("UI Manager configured to use Termux-GUI fallback renderer");

    } else {
        // GLES2 рендерер инициализирован успешно
        log_info("GLES2 renderer initialized successfully");
        // Создаем UI manager с GLES2 рендерером
        g_app.ui_manager = ui_manager_create(g_app.renderer);
        if (!g_app.ui_manager) {
            log_error("Failed to initialize UI manager with GLES2 renderer");
            goto cleanup;
        }
        log_info("UI Manager configured to use GLES2 renderer");
    }
    // --- КОНЕЦ ЛОГИКИ ИНИЦИАЛИЗАЦИИ РЕНДЕРЕРА С FALLBACK ---


    // Initialize socket server
    g_app.socket_server = socket_server_create(config->socket_path, on_socket_data);
    if (!g_app.socket_server) {
        log_error("Failed to initialize socket server");
        goto cleanup;
    }

    // Start socket server thread
    if (pthread_create(&g_app.socket_thread, NULL,
                      (void*(*)(void*))socket_server_run, g_app.socket_server) != 0) {
        log_error("Failed to create socket thread");
        goto cleanup;
    }

    g_app.running = 1;
    g_app.initialized = 1;
    g_app.needs_redraw = 1;

    log_info("Application initialization completed");
    return 1;

cleanup:
    app_cleanup();
    return 0;
}

// --- ИЗМЕНЕННАЯ ФУНКЦИЯ ---
int app_update(void) {
    if (!g_app.initialized || !g_app.running) {
        return 0;
    }

    pthread_mutex_lock(&g_app.state_mutex);

    if (ui_manager_get_renderer_type(g_app.ui_manager) == RENDERER_TYPE_GLES2 && g_app.renderer) {
        // Начинаем формирование батча
        renderer_begin_frame(g_app.renderer);
        // Очищаем фон
        renderer_clear(g_app.renderer, 0.06f, 0.06f, 0.06f, 1.0f);
    }

    if (g_app.needs_redraw) {
        ui_manager_update_layout(g_app.ui_manager, g_app.scroll_y);
        g_app.needs_redraw = 0;
    }

    // Эта функция теперь не рисует, а только добавляет вершины в батч
    ui_manager_render(g_app.ui_manager);

    if (ui_manager_get_renderer_type(g_app.ui_manager) == RENDERER_TYPE_GLES2 && g_app.renderer) {
        // Завершаем кадр, что вызовет flush и swap buffers
        if (!renderer_end_frame(g_app.renderer)) {
            log_error("Failed to end renderer frame");
            pthread_mutex_unlock(&g_app.state_mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&g_app.state_mutex);
    return 1;
}
// --- КОНЕЦ ИЗМЕНЕНИЙ ---

void app_shutdown(void) {
    log_info("Initiating application shutdown");
    g_app.running = 0;

    // Stop socket server
    if (g_app.socket_server) {
        socket_server_stop(g_app.socket_server);
    }
}

void app_cleanup(void) {
    if (!g_app.initialized) {
        return;
    }

    log_info("Cleaning up application resources");

    // Wait for socket thread to finish
    if (g_app.socket_thread) {
        pthread_join(g_app.socket_thread, NULL);
    }

    // Clean up components in reverse order
    if (g_app.socket_server) {
        socket_server_destroy(g_app.socket_server);
        g_app.socket_server = NULL;
    }
    if (g_app.ui_manager) {
        ui_manager_destroy(g_app.ui_manager);
        g_app.ui_manager = NULL;
    }
    if (g_app.renderer) {
        renderer_destroy(g_app.renderer);
        g_app.renderer = NULL;
    }
    if (g_app.diff_data) {
        diff_data_destroy(g_app.diff_data);
        g_app.diff_data = NULL;
    }
    if (g_app.termux_backend) {
        termux_gui_backend_destroy(g_app.termux_backend);
        g_app.termux_backend = NULL;
    }

    // Clean up threading
    pthread_mutex_destroy(&g_app.state_mutex);
    g_app.initialized = 0;
    g_app.running = 0;

    log_info("Application cleanup completed");
}

int app_is_running(void) {
    return g_app.running;
}

const AppConfig* app_get_config(void) {
    return g_app.initialized ? &g_app.config : NULL;
}

void app_handle_touch(float x, float y) {
    if (!g_app.initialized) return;

    pthread_mutex_lock(&g_app.state_mutex);
    log_debug("Touch event at (%.2f, %.2f)", x, y);

    // Convert touch coordinates and handle UI interaction
    // ui_manager expects y coordinate adjusted by scroll
    if (ui_manager_handle_touch(g_app.ui_manager, x, y + g_app.scroll_y)) {
        g_app.needs_redraw = 1;
        log_debug("Touch handled by UI manager");
    }

    pthread_mutex_unlock(&g_app.state_mutex);
}

void app_handle_scroll(float dx, float dy) {
    if (!g_app.initialized) return;

    pthread_mutex_lock(&g_app.state_mutex);

    g_app.scroll_y += dy * SCROLL_SENSITIVITY;

    // Clamp scroll position
    if (g_app.scroll_y < 0) {
        g_app.scroll_y = 0;
    }
    float max_scroll = ui_manager_get_content_height(g_app.ui_manager) - g_app.config.window_height;
    if (max_scroll > 0 && g_app.scroll_y > max_scroll) {
        g_app.scroll_y = max_scroll;
    }

    g_app.needs_redraw = 1;
    log_debug("Scroll updated: y=%.2f", g_app.scroll_y);

    pthread_mutex_unlock(&g_app.state_mutex);
}

void app_handle_resize(int width, int height) {
    if (!g_app.initialized) return;

    pthread_mutex_lock(&g_app.state_mutex);

    log_info("Window resized to %dx%d", width, height);
    g_app.config.window_width = width;
    g_app.config.window_height = height;

    // Update renderer if it's GLES2
    if (ui_manager_get_renderer_type(g_app.ui_manager) == RENDERER_TYPE_GLES2 && g_app.renderer) {
        renderer_resize(g_app.renderer, width, height);
    }

    g_app.needs_redraw = 1;

    pthread_mutex_unlock(&g_app.state_mutex);
}
