// src/core/app.c
#include "see_code/core/app.h"
#include "see_code/core/config.h"
#include "see_code/network/socket_server.h"
#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"
#include "see_code/gui/termux_gui_backend.h" // Для критического fallback
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/renderer.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Для usleep

#define SCROLL_SENSITIVITY 20.0f

// Forward declarations for internal use
static void* socket_thread_func(void* arg);
static void on_socket_data(const char* data_buffer, size_t length);

// --- Глобальное состояние приложения ---
// Это упрощает доступ к состоянию из разных функций,
// но в более крупных проектах можно рассмотреть передачу указателя на AppState.
static struct {
    AppConfig config;
    int running;
    int initialized;
    // Component handles
    SocketServer* socket_server;
    Renderer* renderer;             // GLES2 renderer
    UIManager* ui_manager;
    DiffData* diff_data;
    TermuxGUIBackend* termux_backend; // Backend для критического fallback
    // Threading
    pthread_mutex_t state_mutex;
    pthread_t socket_thread;
    // Application state
    float scroll_y;
    int needs_redraw;
} g_app = {0}; // Инициализируем всё нулями

// --- Вспомогательная функция для проверки состояния текстового рендерера ---
// Проверяет, был ли текстовый рендерер успешно инициализирован внутри GLES2 рендерера.
// Это критично, потому что если текст не рендерится (только прямоугольники), это неприемлемо.
static int is_text_renderer_usable(const Renderer* renderer) {
    if (!renderer) {
        return 0; // Нет рендерера - нет текста
    }
    
    // Получаем указатель на внутренние данные текстового рендерера.
    // Предполагается, что renderer->text_internal_data_private указывает на структуру,
    // определенную в renderer.c/text_renderer.c, например, TextRendererInternalData.
    // Эта структура должна содержать флаг is_freetype_initialized.
    void* text_internal_data = renderer->text_internal_data_private;
    
    if (text_internal_data) {
        // Приводим к предполагаемому типу. Нужно убедиться, что это соответствует реальной структуре.
        // Если структура называется иначе, нужно изменить.
        struct TextRendererInternalData* tr_data = (struct TextRendererInternalData*)text_internal_data;
        
        // Проверяем флаг, установленный в text_renderer_init.
        // Если is_freetype_initialized == 1, значит FreeType был успешно инициализирован
        // с каким-либо шрифтом (основным или fallback).
        // Если is_freetype_initialized == 0, значит текст рендерится placeholder-ами.
        if (tr_data->is_freetype_initialized) {
            return 1; // Текст инициализирован и пригоден к использованию
        }
    }
    // Если указатель NULL или флаг не установлен
    return 0;
}

// --- Основная логика инициализации приложения ---
static int app_init_internal(const AppConfig* config) {
    if (!config) {
        log_error("Invalid configuration provided to app_init_internal");
        return 0;
    }

    // 1. Инициализируем состояние приложения
    memset(&g_app, 0, sizeof(g_app)); // Очищаем всё перед началом
    g_app.config = *config; // Копируем конфиг
    g_app.scroll_y = 0.0f;
    g_app.needs_redraw = 1;
    g_app.running = 0; // Пока не запущено
    g_app.initialized = 0; // Пока не инициализировано

    // Инициализируем мьютекс для синхронизации доступа к состоянию
    if (pthread_mutex_init(&g_app.state_mutex, NULL) != 0) {
        log_error("Failed to initialize state mutex");
        // Не переходим к cleanup, так как мьютекс не был инициализирован
        return 0;
    }

    // 2. Создаем контейнер для данных diff
    g_app.diff_data = diff_data_create();
    if (!g_app.diff_data) {
        log_error("Failed to create diff data container");
        goto cleanup; // Переход к освобождению ресурсов
    }

    // --- ЛОГИКА ИНИЦИАЛИЗАЦИИ ГРАФИЧЕСКОЙ ПОДСИСТЕМЫ ---
    log_info("Attempting to initialize primary GLES2 renderer...");
    
    // Попытка 1: Инициализация основного GLES2 рендерера
    g_app.renderer = renderer_create(config->window_width, config->window_height);
    
    if (g_app.renderer) {
        // GLES2 рендерер был создан. Теперь проверим, можно ли с ним рисовать текст.
        if (is_text_renderer_usable(g_app.renderer)) {
            log_info("GLES2 renderer with usable text initialized successfully.");
            // И GLES2, и текст в порядке. Создаем UI manager.
            g_app.ui_manager = ui_manager_create(g_app.renderer);
            if (!g_app.ui_manager) {
                 log_error("Failed to create UI manager with GLES2 renderer");
                 // Удаляем рендерер, так как им не получится воспользоваться
                 renderer_destroy(g_app.renderer);
                 g_app.renderer = NULL;
                 // Переходим к попытке Termux-GUI
            } else {
                // Всё хорошо, GLES2 и UI работают. Переходим к остальной инициализации.
                goto renderer_success;
            }
        } else {
            // GLES2 есть, но текста нет (is_freetype_initialized == 0).
            // Это неприемлемо как основной режим.
            log_warn("GLES2 renderer initialized, but text rendering is not available (using placeholders).");
            log_warn("This is not acceptable as primary renderer. Falling back to Termux-GUI.");
            // Удаляем непригодный рендерер
            renderer_destroy(g_app.renderer);
            g_app.renderer = NULL;
            // Переходим к попытке Termux-GUI
        }
    } else {
        // GLES2 рендерер вообще не создался.
        log_warn("Failed to initialize primary GLES2 renderer.");
        // Переходим к попытке Termux-GUI
    }
    
    // --- ПОПЫТКА ИНИЦИАЛИЗАЦИИ TERMUX-GUI КАК КРИТИЧЕСКОЙ АЛЬТЕРНАТИВЫ ---
    log_info("Attempting Termux-GUI as critical fallback...");
    if (termux_gui_backend_is_available()) {
        log_info("Termux-GUI library is available");
        g_app.termux_backend = termux_gui_backend_create();
        if (g_app.termux_backend) {
            if (termux_gui_backend_init(g_app.termux_backend)) {
                log_info("Termux GUI backend initialized successfully.");
                // Termux-GUI удался, создаем UI manager с NULL 
                // (ui_manager_create должен переключиться на fallback).
                g_app.ui_manager = ui_manager_create(NULL);
                if (!g_app.ui_manager) {
                     log_error("Failed to create UI manager for Termux-GUI fallback");
                     termux_gui_backend_destroy(g_app.termux_backend);
                     g_app.termux_backend = NULL;
                     // Переходим к финальному этапу ошибки
                } else {
                    // Всё хорошо, Termux-GUI и UI работают. Переходим к остальной инициализации.
                    goto renderer_success;
                }
            } else {
                log_error("Failed to initialize Termux GUI backend connection/activity");
                termux_gui_backend_destroy(g_app.termux_backend);
                g_app.termux_backend = NULL;
                // Переходим к финальному этапу ошибки
            }
        } else {
            log_error("Failed to create Termux GUI backend instance");
            // Переходим к финальному этапу ошибки
        }
    } else {
        log_info("Termux-GUI library is not available.");
        // Переходим к финальному этапу ошибки
    }
    
    // --- ФИНАЛЬНЫЙ ЭТАП ОШИБКИ ---
    log_error("CRITICAL: Failed to initialize any usable rendering system (GLES2 with text or Termux-GUI). Cannot proceed.");
    goto cleanup; // Переход к cleanup в случае критической ошибки

renderer_success:
    // Этот блок достигается, если один из рендереров 
    // (GLES2 с текстом или Termux-GUI) успешно инициализирован
    // и g_app.ui_manager создан.
    log_info("Renderer (either GLES2+Text or Termux-GUI) and UI Manager initialized successfully.");
    // --- КОНЕЦ ЛОГИКИ ИНИЦИАЛИЗАЦИИ ГРАФИКИ ---

    // 3. Создаем и запускаем сервер сокетов
    g_app.socket_server = socket_server_create(config->socket_path, on_socket_data);
    if (!g_app.socket_server) {
        log_error("Failed to create socket server");
        goto cleanup;
    }

    if (socket_server_start(g_app.socket_server) != 0) {
        log_error("Failed to start socket server");
        goto cleanup;
    }

    // 4. Создаем поток для обработки сокетов
    if (pthread_create(&g_app.socket_thread, NULL, socket_thread_func, NULL) != 0) {
        log_error("Failed to create socket thread");
        goto cleanup;
    }

    // 5. Финальная настройка состояния приложения
    g_app.running = 1;
    g_app.initialized = 1;
    g_app.needs_redraw = 1;
    log_info("Application initialization completed successfully");
    return 1; // УСПЕХ

// Метка для освобождения ресурсов в случае ошибки
cleanup:
    log_warn("Cleaning up resources due to initialization failure...");
    
    // Освобождаем ресурсы в порядке, обратном созданию
    if (g_app.socket_server) {
        socket_server_stop(g_app.socket_server); // Останавливаем сервер
        socket_server_destroy(g_app.socket_server); // Уничтожаем сервер
        g_app.socket_server = NULL;
    }
    
    // Если поток был создан, ждем его завершения
    if (g_app.socket_thread) {
         // Отменяем поток, если он еще работает
         pthread_cancel(g_app.socket_thread);
         // Ждем завершения (pthread_join может блокировать, если поток не завершается)
         // В реальном приложении лучше использовать флаг остановки и pthread_join с таймаутом
         pthread_join(g_app.socket_thread, NULL);
         g_app.socket_thread = 0;
    }
    
    if (g_app.ui_manager) {
        ui_manager_destroy(g_app.ui_manager);
        g_app.ui_manager = NULL;
    }
    
    // Уничтожаем рендерер, если он был создан, но не подошел
    if (g_app.renderer) {
        renderer_destroy(g_app.renderer);
        g_app.renderer = NULL;
    }
    
    // Уничтожаем backend Termux-GUI, если он был создан, но не подошел
    if (g_app.termux_backend) {
        termux_gui_backend_destroy(g_app.termux_backend);
        g_app.termux_backend = NULL;
    }
    
    if (g_app.diff_data) {
        diff_data_destroy(g_app.diff_data);
        g_app.diff_data = NULL;
    }
    
    // Уничтожаем мьютекс
    pthread_mutex_destroy(&g_app.state_mutex);
    
    // Полная очистка состояния
    memset(&g_app, 0, sizeof(g_app));
    log_error("Application initialization failed and resources cleaned up.");
    return 0; // ОШИБКА
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
    if (g_app.socket_thread) {
        // Отменяем поток
        pthread_cancel(g_app.socket_thread);
        // Ждем его завершения
        pthread_join(g_app.socket_thread, NULL);
        g_app.socket_thread = 0;
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

// --- Обновление и рендеринг ---
void app_update(float delta_time) {
    if (!g_app.initialized || !g_app.running) {
        return;
    }

    // Обновляем UI manager
    if (g_app.ui_manager) {
        ui_manager_update(g_app.ui_manager, delta_time);
    }
}

int app_render() {
    if (!g_app.initialized || !g_app.running) {
        return 0;
    }

    int frame_rendered = 0;

    // Определяем тип рендерера и вызываем соответствующую функцию рендеринга
    RendererType current_renderer_type = RENDERER_TYPE_UNKNOWN;
    if (g_app.ui_manager) {
        current_renderer_type = ui_manager_get_renderer_type(g_app.ui_manager);
    }

    if (current_renderer_type == RENDERER_TYPE_GLES2) {
        if (g_app.renderer && g_app.ui_manager) {
            renderer_begin_frame(g_app.renderer);
            
            // Рендерим UI
            ui_manager_render(g_app.ui_manager);
            
            // Рендерим виджеты (если они будут добавлены)
            // widgets_render(...);
            
            frame_rendered = renderer_end_frame(g_app.renderer);
        }
    } else if (current_renderer_type == RENDERER_TYPE_TERMUX_GUI) {
        if (g_app.termux_backend && g_app.ui_manager) {
            // Рендерим через Termux-GUI backend
            // Предполагается, что ui_manager_render внутри переключается
            // на termux_gui_backend_render_diff, если рендерер NULL.
            ui_manager_render(g_app.ui_manager);
            frame_rendered = 1; // Считаем, что кадр отрендерен
        }
    } else {
        log_error("Unknown or unsupported renderer type during render call");
    }

    return frame_rendered;
}

// --- Обработка ввода ---
// --- ОБНОВЛЕННАЯ ФУНКЦИЯ ОБРАБОТКИ КЛАВИШ ---
void app_handle_key(int key_code) {
    if (!g_app.initialized || !g_app.running) {
        return;
    }

    pthread_mutex_lock(&g_app.state_mutex);
    // Передаем событие в UI manager
    // Теперь ui_manager_handle_key должен обрабатывать ввод для виджетов
    if (g_app.ui_manager) {
        ui_manager_handle_key(g_app.ui_manager, key_code);
        g_app.needs_redraw = 1;
    }
    pthread_mutex_unlock(&g_app.state_mutex);
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ ---

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

void app_handle_touch(float x, float y) {
    if (!g_app.initialized || !g_app.running) {
        return;
    }

    pthread_mutex_lock(&g_app.state_mutex);
    // Передаем событие в UI manager
    if (g_app.ui_manager) {
        ui_manager_handle_touch(g_app.ui_manager, x, y);
        g_app.needs_redraw = 1;
    }
    pthread_mutex_unlock(&g_app.state_mutex);
}

// --- Функции для работы с данными ---
void app_set_diff_data(const DiffData* data) {
    if (!g_app.initialized || !g_app.running || !data) {
        return;
    }

    pthread_mutex_lock(&g_app.state_mutex);
    if (g_app.diff_data) {
        // Обновляем данные в существующем контейнере
        // Предполагается, что diff_data_update или подобная функция существует
        // или мы очищаем и копируем. Пока используем очистку и загрузку.
        diff_data_clear(g_app.diff_data);
        // Копирование данных - это сложная операция, 
        // лучше передавать владение или использовать ссылки.
        // Пока просто заглушка.
        log_warn("app_set_diff_data: Full data copy not implemented, using placeholder logic.");
    }
    g_app.needs_redraw = 1;
    pthread_mutex_unlock(&g_app.state_mutex);
}

const DiffData* app_get_diff_data() {
    return g_app.diff_data;
}

// --- Сетевой слой ---

// Потоковая функция для сервера сокетов
static void* socket_thread_func(void* arg) {
    (void)arg; // Неиспользуемый параметр
    log_info("Socket server thread started");
    
    // Запускаем цикл обработки соединений
    if (g_app.socket_server) {
        socket_server_run(g_app.socket_server);
    }
    
    log_info("Socket server thread finished");
    return NULL;
}

// Callback, вызываемый сервером сокетов при получении данных
static void on_socket_data(const char* data_buffer, size_t length) {
    log_info("Received %zu raw bytes from client", length);
    
    pthread_mutex_lock(&g_app.state_mutex);

    // Загружаем данные из буфера с помощью парсера
    if (g_app.diff_data && diff_data_load_from_buffer(g_app.diff_data, data_buffer, length)) {
        log_info("Successfully loaded data from raw buffer");
        g_app.needs_redraw = 1;
        // Обновляем UI с новыми данными
        if (g_app.ui_manager) {
            ui_manager_set_diff_data(g_app.ui_manager, g_app.diff_data);
        }
    } else {
        log_error("Failed to load diff data from buffer");
        // Можно отобразить сообщение об ошибке в UI
    }

    pthread_mutex_unlock(&g_app.state_mutex);
}
