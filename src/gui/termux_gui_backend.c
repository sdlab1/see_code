// src/gui/termux_gui_backend.c
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include "see_code/data/diff_data.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Dynamically loaded function pointers for termux-gui-c
static void* g_termux_gui_lib = NULL;

// Function pointer types
typedef void* (*tgui_connection_create_t)(void);
typedef void (*tgui_connection_destroy_t)(void*);
typedef void* (*tgui_activity_create_t)(void*, int);
typedef void (*tgui_activity_destroy_t)(void*);
typedef void* (*tgui_textview_create_t)(void*, const char*);
typedef void* (*tgui_button_create_t)(void*, const char*);
typedef void (*tgui_view_set_position_t)(void*, int, int, int, int);
typedef void (*tgui_view_set_text_size_t)(void*, int);
typedef void (*tgui_view_set_text_color_t)(void*, uint32_t);
typedef void (*tgui_view_set_id_t)(void*, int);
typedef void (*tgui_clear_views_t)(void*);
typedef void (*tgui_activity_set_orientation_t)(void*, int);
typedef void (*tgui_wait_for_events_t)(void*, int);
typedef void (*tgui_free_event_t)(void*);
typedef void* (*tgui_get_event_t)(void*);
typedef int (*tgui_event_get_type_t)(void*);
typedef void* (*tgui_event_get_view_t)(void*);
typedef int (*tgui_view_get_id_t)(void*);

// Global function pointers
static tgui_connection_create_t g_tgui_connection_create = NULL;
static tgui_connection_destroy_t g_tgui_connection_destroy = NULL;
static tgui_activity_create_t g_tgui_activity_create = NULL;
static tgui_activity_destroy_t g_tgui_activity_destroy = NULL;
static tgui_textview_create_t g_tgui_textview_create = NULL;
static tgui_button_create_t g_tgui_button_create = NULL;
static tgui_view_set_position_t g_tgui_view_set_position = NULL;
static tgui_view_set_text_size_t g_tgui_view_set_text_size = NULL;
static tgui_view_set_text_color_t g_tgui_view_set_text_color = NULL;
static tgui_view_set_id_t g_tgui_view_set_id = NULL;
static tgui_clear_views_t g_tgui_clear_views = NULL;
static tgui_activity_set_orientation_t g_tgui_activity_set_orientation = NULL;
static tgui_wait_for_events_t g_tgui_wait_for_events = NULL;
static tgui_free_event_t g_tgui_free_event = NULL;
static tgui_get_event_t g_tgui_get_event = NULL;
static tgui_event_get_type_t g_tgui_event_get_type = NULL;
static tgui_event_get_view_t g_tgui_event_get_view = NULL;
static tgui_view_get_id_t g_tgui_view_get_id = NULL;

struct TermuxGUIBackend {
    void* conn; // tgui_connection*
    void* activity; // tgui_activity*
    int initialized;
    int view_counter; // Simple ID counter for views
};

int termux_gui_backend_is_available(void) {
    if (g_termux_gui_lib) {
        return 1; // Already loaded
    }

    // Try to load the library
    g_termux_gui_lib = dlopen("libtermux-gui.so", RTLD_LAZY);
    if (!g_termux_gui_lib) {
        // Try alternative name
        g_termux_gui_lib = dlopen("libtermux-gui-c.so", RTLD_LAZY);
    }

    if (!g_termux_gui_lib) {
        log_warn("termux-gui-c library not found for fallback: %s", dlerror());
        return 0;
    }

    // Load essential symbols
    g_tgui_connection_create = (tgui_connection_create_t) dlsym(g_termux_gui_lib, "tgui_connection_create");
    g_tgui_connection_destroy = (tgui_connection_destroy_t) dlsym(g_termux_gui_lib, "tgui_connection_destroy");
    g_tgui_activity_create = (tgui_activity_create_t) dlsym(g_termux_gui_lib, "tgui_activity_create");
    g_tgui_activity_destroy = (tgui_activity_destroy_t) dlsym(g_termux_gui_lib, "tgui_activity_destroy");
    g_tgui_textview_create = (tgui_textview_create_t) dlsym(g_termux_gui_lib, "tgui_textview_create");
    g_tgui_button_create = (tgui_button_create_t) dlsym(g_termux_gui_lib, "tgui_button_create");
    g_tgui_view_set_position = (tgui_view_set_position_t) dlsym(g_termux_gui_lib, "tgui_view_set_position");
    g_tgui_view_set_text_size = (tgui_view_set_text_size_t) dlsym(g_termux_gui_lib, "tgui_view_set_text_size");
    g_tgui_view_set_text_color = (tgui_view_set_text_color_t) dlsym(g_termux_gui_lib, "tgui_view_set_text_color");
    g_tgui_view_set_id = (tgui_view_set_id_t) dlsym(g_termux_gui_lib, "tgui_view_set_id");
    g_tgui_clear_views = (tgui_clear_views_t) dlsym(g_termux_gui_lib, "tgui_clear_views");
    g_tgui_activity_set_orientation = (tgui_activity_set_orientation_t) dlsym(g_termux_gui_lib, "tgui_activity_set_orientation");
    g_tgui_wait_for_events = (tgui_wait_for_events_t) dlsym(g_termux_gui_lib, "tgui_wait_for_events");
    g_tgui_free_event = (tgui_free_event_t) dlsym(g_termux_gui_lib, "tgui_free_event");
    g_tgui_get_event = (tgui_get_event_t) dlsym(g_termux_gui_lib, "tgui_get_event");
    g_tgui_event_get_type = (tgui_event_get_type_t) dlsym(g_termux_gui_lib, "tgui_event_get_type");
    g_tgui_event_get_view = (tgui_event_get_view_t) dlsym(g_termux_gui_lib, "tgui_event_get_view");
    g_tgui_view_get_id = (tgui_view_get_id_t) dlsym(g_termux_gui_lib, "tgui_view_get_id");

    if (!g_tgui_connection_create || !g_tgui_connection_destroy || !g_tgui_activity_create ||
        !g_tgui_textview_create || !g_tgui_button_create || !g_tgui_view_set_position ||
        !g_tgui_clear_views || !g_tgui_activity_set_orientation || !g_tgui_wait_for_events ||
        !g_tgui_free_event || !g_tgui_get_event || !g_tgui_event_get_type || !g_tgui_event_get_view ||
        !g_tgui_view_get_id || !g_tgui_view_set_text_size || !g_tgui_view_set_text_color || !g_tgui_view_set_id) {
        log_error("Failed to load essential termux-gui-c symbols");
        dlclose(g_termux_gui_lib);
        g_termux_gui_lib = NULL;
        g_tgui_connection_create = NULL;
        g_tgui_connection_destroy = NULL;
        g_tgui_activity_create = NULL;
        // ... reset others ...
        return 0;
    }

    log_info("termux-gui-c library loaded successfully for fallback");
    return 1;
}

TermuxGUIBackend* termux_gui_backend_create(void) {
    if (!termux_gui_backend_is_available()) {
        log_error("Cannot create TermuxGUIBackend: library not available");
        return NULL;
    }

    TermuxGUIBackend* backend = malloc(sizeof(TermuxGUIBackend));
    if (!backend) {
        log_error("Failed to allocate memory for TermuxGUIBackend");
        return NULL;
    }
    memset(backend, 0, sizeof(TermuxGUIBackend));
    backend->view_counter = 1; // Start IDs from 1
    return backend;
}

void termux_gui_backend_destroy(TermuxGUIBackend* backend) {
    if (!backend) {
        return;
    }
    if (backend->activity && g_tgui_activity_destroy) {
        g_tgui_activity_destroy(backend->activity);
    }
    if (backend->conn && g_tgui_connection_destroy) {
        g_tgui_connection_destroy(backend->conn);
    }
    free(backend);

    // Optional: Unload library on last backend destruction
    // For simplicity, we keep it loaded once loaded.
    // if (g_termux_gui_lib) { dlclose(g_termux_gui_lib); g_termux_gui_lib = NULL; ... }
}

int termux_gui_backend_init(TermuxGUIBackend* backend) {
    if (!backend || !termux_gui_backend_is_available()) {
        return 0;
    }

    if (backend->initialized) {
        return 1;
    }

    backend->conn = g_tgui_connection_create();
    if (!backend->conn) {
        log_error("Failed to create termux-gui connection");
        return 0;
    }

    backend->activity = g_tgui_activity_create(backend->conn, 0); // 0 for default/no flags
    if (!backend->activity) {
        log_error("Failed to create termux-gui activity");
        g_tgui_connection_destroy(backend->conn);
        backend->conn = NULL;
        return 0;
    }

    // Set orientation
    g_tgui_activity_set_orientation(backend->activity, 1); // 1 for landscape, 0 for portrait

    backend->initialized = 1;
    log_info("TermuxGUIBackend initialized successfully");
    return 1;
}

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ РЕНДЕРИНГА ---
void termux_gui_backend_render_diff(TermuxGUIBackend* backend, const DiffData* data) {
    if (!backend || !backend->initialized || !data) {
        return;
    }

    // Очищаем предыдущие представления
    g_tgui_clear_views(backend->activity);

    int y_pos = 50; // Начальная позиция Y
    const int x_margin = 10;
    const int line_height = 20;
    const int hunk_header_height = 25;
    const int file_header_height = 30;
    const int screen_width = 1080; // Предполагаемая ширина экрана
    int view_id_counter = 1; // Простой счетчик ID для представлений

    for (size_t i = 0; i < data->file_count; i++) {
        const DiffFile* file = &data->files[i];
        if (file->path) {
            // Создаем TextView для пути файла
            void* file_header_view = g_tgui_textview_create(backend->activity, file->path);
            if (file_header_view) {
                g_tgui_view_set_position(file_header_view, x_margin, y_pos, screen_width - 2 * x_margin, file_header_height);
                g_tgui_view_set_text_size(file_header_view, 18); // Крупный шрифт для заголовков файлов
                g_tgui_view_set_text_color(file_header_view, 0xFF4444FF); // Синеватый
                g_tgui_view_set_id(file_header_view, view_id_counter++);
                // В реальном приложении можно добавить обработку кликов для сворачивания/разворачивания
            }
            log_debug("Rendering file: %s (Fallback)", file->path);
        }
        y_pos += file_header_height + 10; // Отступ

        for (size_t j = 0; j < file->hunk_count; j++) {
            const DiffHunk* hunk = &file->hunks[j];
            if (hunk->header) {
                // Создаем TextView для заголовка ханка
                void* hunk_header_view = g_tgui_textview_create(backend->activity, hunk->header);
                if (hunk_header_view) {
                     g_tgui_view_set_position(hunk_header_view, x_margin + 10, y_pos, screen_width - 2 * (x_margin + 10), hunk_header_height);
                     g_tgui_view_set_text_size(hunk_header_view, 14);
                     g_tgui_view_set_text_color(hunk_header_view, 0xFF000000); // Черный
                     g_tgui_view_set_id(hunk_header_view, view_id_counter++);
                     // Можно сделать кнопкой для сворачивания
                }
                log_debug("  Rendering hunk: %s (Fallback)", hunk->header);
            }
            y_pos += hunk_header_height + 5;

            // Рендерим строки
            for (size_t k = 0; k < hunk->line_count; k++) {
                const DiffLine* line = &hunk->lines[k];
                if (line->content) {
                    // Ограничиваем длину строки для отображения
                    char* display_content = NULL;
                    if (line->length > 100) {
                        display_content = malloc(100 + 4); // +4 для "..."
                        if (display_content) {
                            strncpy(display_content, line->content, 100);
                            strcpy(display_content + 100, "...");
                        }
                    } else {
                        display_content = strdup(line->content);
                    }

                    void* line_view = g_tgui_textview_create(backend->activity, display_content ? display_content : line->content);
                    free(display_content); // Освобождаем временную строку

                    if (line_view) {
                        g_tgui_view_set_position(line_view, x_margin + 20, y_pos, screen_width - 2 * (x_margin + 20), line_height);
                        g_tgui_view_set_text_size(line_view, 12);

                        // Устанавливаем цвет в зависимости от типа строки
                        uint32_t color = 0xFF000000; // По умолчанию черный
                        if (line->type == LINE_TYPE_ADD) {
                            color = 0xFF00AA00; // Зеленый
                        } else if (line->type == LINE_TYPE_DELETE) {
                            color = 0xFFAA0000; // Красный
                        } else if (line->type == LINE_TYPE_CONTEXT) {
                            color = 0xFF888888; // Серый
                        }
                        g_tgui_view_set_text_color(line_view, color);
                        g_tgui_view_set_id(line_view, view_id_counter++);
                    }
                    log_debug("    Line (%d): %s (Fallback)", line->type, line->content);
                    y_pos += line_height + 2;
                }
            }
            y_pos += 10; // Дополнительный отступ между ханками
        }
        y_pos += 20; // Дополнительный отступ между файлами
    }
    log_info("Diff rendered using Termux GUI backend (Fallback)");
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ РЕНДЕРИНГА ---

// Placeholder for event handling - in a real app, this would be a loop
void termux_gui_backend_handle_events(TermuxGUIBackend* backend) {
     if (!backend || !backend->initialized) {
        return;
    }
    // This is a simplified non-blocking check for demonstration.
    // A real implementation would have a dedicated thread or integrate into the main loop.
    // For now, we'll just log that event handling is possible.
    log_debug("Termux GUI backend can handle events (Fallback)");
    // Example of how it might look in a loop:
    /*
    void* event = g_tgui_get_event(backend->conn);
    if (event) {
        int type = g_tgui_event_get_type(event);
        if (type == TGUI_EVENT_TYPE_CLICK) {
            void* view = g_tgui_event_get_view(event);
            int id = g_tgui_view_get_id(view);
            // Handle click on view with ID 'id'
            log_info("Clicked on view ID: %d", id);
        }
        g_tgui_free_event(event);
    }
    */
}
