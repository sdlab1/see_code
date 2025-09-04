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

// Function pointer types (минимальный необходимый набор)
typedef void* (*tgui_connection_create_t)(void);
typedef void (*tgui_connection_destroy_t)(void*);
typedef void* (*tgui_activity_create_t)(void*, int);
typedef void (*tgui_activity_destroy_t)(void*);
typedef void* (*tgui_textview_create_t)(void*, const char*);
typedef void (*tgui_view_set_position_t)(void*, int, int, int, int);
typedef void (*tgui_view_set_text_size_t)(void*, int);
typedef void (*tgui_view_set_text_color_t)(void*, uint32_t);
typedef void (*tgui_clear_views_t)(void*);
typedef void (*tgui_activity_set_orientation_t)(void*, int);

// Global function pointers
static tgui_connection_create_t g_tgui_connection_create = NULL;
static tgui_connection_destroy_t g_tgui_connection_destroy = NULL;
static tgui_activity_create_t g_tgui_activity_create = NULL;
static tgui_activity_destroy_t g_tgui_activity_destroy = NULL;
static tgui_textview_create_t g_tgui_textview_create = NULL;
static tgui_view_set_position_t g_tgui_view_set_position = NULL;
static tgui_view_set_text_size_t g_tgui_view_set_text_size = NULL;
static tgui_view_set_text_color_t g_tgui_view_set_text_color = NULL;
static tgui_clear_views_t g_tgui_clear_views = NULL;
static tgui_activity_set_orientation_t g_tgui_activity_set_orientation = NULL;

struct TermuxGUIBackend {
    void* conn;
    void* activity;
    int initialized;
};

int termux_gui_backend_is_available(void) {
    if (g_termux_gui_lib) {
        return 1;
    }

    g_termux_gui_lib = dlopen("libtermux-gui.so", RTLD_LAZY);
    if (!g_termux_gui_lib) {
        g_termux_gui_lib = dlopen("libtermux-gui-c.so", RTLD_LAZY);
    }

    if (!g_termux_gui_lib) {
        log_warn("termux-gui-c library not found for fallback");
        return 0;
    }

    // Load essential symbols
    g_tgui_connection_create = (tgui_connection_create_t) dlsym(g_termux_gui_lib, "tgui_connection_create");
    g_tgui_connection_destroy = (tgui_connection_destroy_t) dlsym(g_termux_gui_lib, "tgui_connection_destroy");
    g_tgui_activity_create = (tgui_activity_create_t) dlsym(g_termux_gui_lib, "tgui_activity_create");
    g_tgui_activity_destroy = (tgui_activity_destroy_t) dlsym(g_termux_gui_lib, "tgui_activity_destroy");
    g_tgui_textview_create = (tgui_textview_create_t) dlsym(g_termux_gui_lib, "tgui_textview_create");
    g_tgui_view_set_position = (tgui_view_set_position_t) dlsym(g_termux_gui_lib, "tgui_view_set_position");
    g_tgui_view_set_text_size = (tgui_view_set_text_size_t) dlsym(g_termux_gui_lib, "tgui_view_set_text_size");
    g_tgui_view_set_text_color = (tgui_view_set_text_color_t) dlsym(g_termux_gui_lib, "tgui_view_set_text_color");
    g_tgui_clear_views = (tgui_clear_views_t) dlsym(g_termux_gui_lib, "tgui_clear_views");
    g_tgui_activity_set_orientation = (tgui_activity_set_orientation_t) dlsym(g_termux_gui_lib, "tgui_activity_set_orientation");

    if (!g_tgui_connection_create || !g_tgui_connection_destroy || !g_tgui_activity_create ||
        !g_tgui_activity_destroy || !g_tgui_textview_create || !g_tgui_view_set_position ||
        !g_tgui_view_set_text_size || !g_tgui_view_set_text_color || !g_tgui_clear_views ||
        !g_tgui_activity_set_orientation) {
        log_error("Failed to load essential termux-gui-c symbols");
        dlclose(g_termux_gui_lib);
        g_termux_gui_lib = NULL;
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
    // Не выгружаем библиотеку, оставляем её загруженной
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

    backend->activity = g_tgui_activity_create(backend->conn, 0);
    if (!backend->activity) {
        log_error("Failed to create termux-gui activity");
        g_tgui_connection_destroy(backend->conn);
        backend->conn = NULL;
        return 0;
    }

    g_tgui_activity_set_orientation(backend->activity, 1); // Landscape

    backend->initialized = 1;
    log_info("TermuxGUIBackend initialized successfully");
    return 1;
}

// --- ФУНКЦИЯ РЕНДЕРИНГА (БАЗОВАЯ) ---
void termux_gui_backend_render_diff(TermuxGUIBackend* backend, const DiffData* data) {
    if (!backend || !backend->initialized || !data) {
        return;
    }

    g_tgui_clear_views(backend->activity);

    int y_pos = 50;
    const int x_margin = 10;
    const int line_height = 20;
    const int hunk_header_height = 25;
    const int file_header_height = 30;
    const int screen_width = 1080;

    for (size_t i = 0; i < data->file_count; i++) {
        const DiffFile* file = &data->files[i];
        if (file->path) {
            void* file_view = g_tgui_textview_create(backend->activity, file->path);
            if (file_view) {
                g_tgui_view_set_position(file_view, x_margin, y_pos, screen_width - 2 * x_margin, file_header_height);
                g_tgui_view_set_text_size(file_view, 18);
                g_tgui_view_set_text_color(file_view, 0xFF4444FF); // Blue
            }
            log_debug("Rendering file: %s", file->path);
        }
        y_pos += file_header_height + 10;

        for (size_t j = 0; j < file->hunk_count; j++) {
            const DiffHunk* hunk = &file->hunks[j];
            if (hunk->header) {
                void* hunk_view = g_tgui_textview_create(backend->activity, hunk->header);
                if (hunk_view) {
                    g_tgui_view_set_position(hunk_view, x_margin + 10, y_pos, screen_width - 2 * (x_margin + 10), hunk_header_height);
                    g_tgui_view_set_text_size(hunk_view, 14);
                    g_tgui_view_set_text_color(hunk_view, 0xFF000000); // Black
                }
                log_debug(" Rendering hunk: %s", hunk->header);
            }
            y_pos += hunk_header_height + 5;

            for (size_t k = 0; k < hunk->line_count; k++) {
                const DiffLine* line = &hunk->lines[k];
                if (line->content) {
                    // Ограничиваем длину для отображения
                    char display_buffer[105]; // 100 chars + "...\0"
                    const char* display_text = line->content;
                    if (line->length > 100) {
                        strncpy(display_buffer, line->content, 100);
                        display_buffer[100] = '\0';
                        strcat(display_buffer, "...");
                        display_text = display_buffer;
                    }

                    void* line_view = g_tgui_textview_create(backend->activity, display_text);
                    if (line_view) {
                        g_tgui_view_set_position(line_view, x_margin + 20, y_pos, screen_width - 2 * (x_margin + 20), line_height);
                        g_tgui_view_set_text_size(line_view, 12);

                        uint32_t color = 0xFF000000; // Default black
                        if (line->type == LINE_TYPE_ADD) {
                            color = 0xFF00AA00; // Green
                        } else if (line->type == LINE_TYPE_DELETE) {
                            color = 0xFFAA0000; // Red
                        } else if (line->type == LINE_TYPE_CONTEXT) {
                            color = 0xFF888888; // Gray
                        }
                        g_tgui_view_set_text_color(line_view, color);
                    }
                    log_debug(" Line (%d): %.50s...", line->type, line->content); // Логируем первые 50 символов
                    y_pos += line_height + 2;
                }
            }
            y_pos += 10;
        }
        y_pos += 20;
    }
    log_info("Diff rendered using Termux GUI backend");
}
// --- КОНЕЦ ФУНКЦИИ РЕНДЕРИНГА ---

void termux_gui_backend_handle_events(TermuxGUIBackend* backend) {
    // Заглушка. Для полноценной работы нужна реализация цикла обработки событий.
    if (!backend || !backend->initialized) {
        return;
    }
    log_debug("Termux GUI backend event handling placeholder");
}
