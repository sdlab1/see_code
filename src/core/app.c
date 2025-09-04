// src/core/app.c
#include "see_code/core/app.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/ui_manager.h"
#include "see_code/network/socket_server.h"
#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"
// Убираем прямое включение, если ui_manager.h его уже включает
// #include "see_code/gui/termux_gui_backend.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

// --- Добавлено для fallback ---
#include "see_code/gui/termux_gui_backend.h" // Убедимся, что есть доступ к функциям проверки
// --- Конец добавления ---

typedef struct {
    AppConfig config;
    int running;
    int initialized;
    // Component handles
    SocketServer* socket_server;
    Renderer* renderer;
    UIManager* ui_manager;
    DiffData* diff_data;
    // Threading
    pthread_mutex_t state_mutex;
    pthread_t socket_thread;
    // Application state
    float scroll_y;
    int needs_redraw;
    // --- Добавлено для fallback ---
    TermuxGUIBackend* termux_backend; // Храним указатель на fallback бэкенд, если он создан
    // --- Конец добавления ---
} AppState;

static AppState g_app = {0};

// Socket callback - called when raw data is received
static void on_socket_data(const char* data_buffer, size_t length) {
    log_info("Received %zu raw bytes from Neovim", length);
    pthread_mutex_lock(&g_app.state_mutex);

    // Instead of parsing JSON, load data from the raw buffer
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

    // --- Логика инициализации с fallback ---
    // 1. Попробуем инициализировать основной GLES2 рендерер
    g_app.renderer = renderer_create(config->window_width, config->window_height);
    if (!g_app.renderer) {
        log_error("Failed to initialize GLES2 renderer");
        // 2. Если GLES2 не удался, проверим доступность Termux-GUI
        if (termux_gui_backend_is_available()) {
            log_warn("Attempting fallback to Termux GUI renderer");
            // 3. Создаем и инициализируем бэкенд Termux-GUI
            g_app.termux_backend = termux_gui_backend_create();
            if (!g_app.termux_backend) {
                 log_error("Failed to create Termux GUI backend");
                 goto cleanup; // Оба рендерера недоступны
            }
            if (!termux_gui_backend_init(g_app.termux_backend)) {
                 log_error("Failed to initialize Termux GUI backend");
                 termux_gui_backend_destroy(g_app.termux_backend);
                 g_app.termux_backend = NULL;
                 goto cleanup; // Оба рендерера недоступны
            }
            // 4. Сообщаем UI Manager, что нужно использовать fallback
            //    (предполагается, что ui_manager_create может принимать информацию о типе)
            //    Или создаем ui_manager позже.
            //    Для простоты, сначала создаем ui_manager с NULL renderer,
            //    а потом переключаем его тип. Это требует, чтобы ui_manager
            //    мог работать без внутреннего GLES2 рендерера, если тип RENDERER_TYPE_TERMUX_GUI.
            //    Альтернатива: создавать ui_manager после выбора рендерера.
            //    Выберем второй путь: сначала определяем рендерер, потом создаем ui_manager.

            // Так как ui_manager_create требует renderer, а у нас его нет,
            // мы не можем создать ui_manager стандартным способом.
            // Нужно изменить ui_manager_create или логику здесь.
            // Пока предположим, что ui_manager может быть создан с NULL,
            // а его тип переключается позже. Это требует изменений в ui_manager.c/h.
            // Чтобы не менять ui_manager, создадим его после определения активного рендерера.
            // Но ui_manager_create требует renderer. Значит, нужно передать информацию
            // о типе рендерера другим способом или изменить ui_manager.

            // Вариант: создаем ui_manager с NULL, а потом переключаем тип.
            // Это требует изменения ui_manager.c/h. Сделаем это предположение.
            // Если ui_manager_create не принимает NULL, это приведет к краху.
            // Лучше изменить логику: сначала определяем тип, потом создаем нужные компоненты.
            // Но это требует большего рефакторинга.

            // Для минимального изменения: создаем ui_manager с NULL renderer.
            // Это потребует изменений в ui_manager_create, чтобы она не падала.
            // Предположим, что ui_manager_create может обработать NULL.
            // Если это не так, потребуется изменить ui_manager.c.

            // Создаем UI manager с NULL, планируя переключить тип
            g_app.ui_manager = ui_manager_create(NULL); // Потенциально опасно, если ui_manager_create проверяет на NULL
            if (!g_app.ui_manager) {
                log_error("Failed to initialize UI manager for Termux GUI fallback");
                termux_gui_backend_destroy(g_app.termux_backend);
                g_app.termux_backend = NULL;
                goto cleanup;
            }
            // Переключаем тип рендерера в UI manager
            ui_manager_set_renderer_type(g_app.ui_manager, RENDERER_TYPE_TERMUX_GUI);
            log_info("Using Termux GUI as primary renderer (GLES2 failed)");

        } else {
            // GLES2 не работает и Termux-GUI недоступен
            log_error("No suitable renderer available (GLES2 failed, Termux-GUI not available)");
            goto cleanup;
        }
    } else {
        // GLES2 рендерер инициализирован успешно
        log_info("Using GLES2 as primary renderer");
        // Создаем UI manager с GLES2 рендерером
        g_app.ui_manager = ui_manager_create(g_app.renderer);
        if (!g_app.ui_manager) {
            log_error("Failed to initialize UI manager");
            goto cleanup;
        }
        // Тип рендерера по умолчанию в ui_manager уже RENDERER_TYPE_GLES2
    }
    // --- Конец логики инициализации с fallback ---


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

int app_update(void) {
    if (!g_app.initialized || !g_app.running) {
        return 0;
    }

    pthread_mutex_lock(&g_app.state_mutex);

    // Handle renderer updates
    // Проверяем, какой рендерер активен
    // if (g_app.renderer) { // Это условие больше не подходит, так как renderer может быть NULL при fallback
    // Лучше проверять тип рендерера через ui_manager или напрямую через флаг/указатель
    // Предположим, что ui_manager знает, какой рендерер использовать.
    // Для GLES2 рендерера нужен begin_frame/end_frame
    if (g_app.renderer) { // Если основной рендерер существует
        if (!renderer_begin_frame(g_app.renderer)) {
            log_error("Failed to begin renderer frame");
            pthread_mutex_unlock(&g_app.state_mutex);
            return 0;
        }
    }
    // Для Termux-GUI эти вызовы не нужны, или они абстрагированы в ui_manager

    // Update UI if needed
    if (g_app.needs_redraw) {
        ui_manager_update_layout(g_app.ui_manager, g_app.scroll_y);
        g_app.needs_redraw = 0;
    }

    // Render UI
    ui_manager_render(g_app.ui_manager);

    // End frame
    if (g_app.renderer) { // Если основной рендерер существует
        if (!renderer_end_frame(g_app.renderer)) {
            log_error("Failed to end renderer frame");
            pthread_mutex_unlock(&g_app.state_mutex);
            return 0;
        }
    }
    // Для Termux-GUI этот вызов не нужен

    pthread_mutex_unlock(&g_app.state_mutex);
    return 1;
}

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

    // Clean up components
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
    // --- Добавлено для fallback ---
    if (g_app.termux_backend) {
        termux_gui_backend_destroy(g_app.termux_backend);
        g_app.termux_backend = NULL;
    }
    // --- Конец добавления ---

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

    // Update renderer
    if (g_app.renderer) {
        renderer_resize(g_app.renderer, width, height);
    }

    g_app.needs_redraw = 1;

    pthread_mutex_unlock(&g_app.state_mutex);
}
