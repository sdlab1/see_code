// src/gui/termux_gui_backend.c
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include "see_code/data/diff_data.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // Для snprintf

// --- Динамически загружаемые функции из libtermux-gui-c ---
static void* g_termux_gui_lib = NULL;

// Function pointer types (минимальный необходимый набор)
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
typedef void (*tgui_view_set_text_t)(void*, const char*); // Для обновления текста TextView

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
static tgui_view_set_text_t g_tgui_view_set_text = NULL; // Добавлено

struct TermuxGUIBackend {
    void* conn; // tgui_connection*
    void* activity; // tgui_activity*
    int initialized;
    // Для обработки событий
    int running; // Флаг, указывающий, запущен ли цикл обработки событий
};

int termux_gui_backend_is_available(void) {
    if (g_termux_gui_lib) {
        return 1; // Уже загружен
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
    g_tgui_view_set_text = (tgui_view_set_text_t) dlsym(g_termux_gui_lib, "tgui_view_set_text"); // Добавлено

    if (!g_tgui_connection_create || !g_tgui_connection_destroy || !g_tgui_activity_create ||
        !g_tgui_activity_destroy || !g_tgui_textview_create || !g_tgui_button_create ||
        !g_tgui_view_set_position || !g_tgui_view_set_text_size || !g_tgui_view_set_text_color ||
        !g_tgui_view_set_id || !g_tgui_clear_views || !g_tgui_activity_set_orientation ||
        !g_tgui_wait_for_events || !g_tgui_free_event || !g_tgui_get_event ||
        !g_tgui_event_get_type || !g_tgui_event_get_view || !g_tgui_view_get_id || !g_tgui_view_set_text) { // Проверяем все необходимые символы
        log_error("Failed to load essential termux-gui-c symbols");
        dlclose(g_termux_gui_lib);
        g_termux_gui_lib = NULL;
        g_tgui_connection_create = NULL;
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
    return backend;
}

void termux_gui_backend_destroy(TermuxGUIBackend* backend) {
    if (!backend) {
        return;
    }
    termux_gui_backend_destroy(backend); // This will call cleanup logic if needed
    free(backend);

    // Unload library only when last backend is destroyed, or keep it loaded for performance
    // For simplicity, we unload it here.
    // A more robust approach would manage library lifetime globally.
    /*
    if (g_termux_gui_lib) {
        dlclose(g_termux_gui_lib);
        g_termux_gui_lib = NULL;
        g_tgui_connection_create = NULL;
        // ... reset others ...
    }
    */
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

    // Set orientation, e.g., landscape
    g_tgui_activity_set_orientation(backend->activity, 1); // 1 for landscape, 0 for portrait

    backend->initialized = 1;
    backend->running = 1; // Mark as running for event loop
    log_info("TermuxGUIBackend initialized successfully");
    return 1;
}

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ РЕНДЕРИНГА ---
void termux_gui_backend_render_diff(TermuxGUIBackend* backend, const DiffData* data) {
    if (!backend || !backend->initialized || !data) {
        return;
    }

    // Clear previous views
    g_tgui_clear_views(backend->activity);

    int y_pos = 50; // Starting Y position
    const int x_margin = 10;
    const int line_height = 20;
    const int hunk_header_height = 25;
    const int file_header_height = 30;
    const int screen_width = 1080; // Assume screen width

    for (size_t i = 0; i < data->file_count; i++) {
        const DiffFile* file = &data->files[i];
        if (file->path) {
            // Create a TextView for the file path
            void* file_header_view = g_tgui_textview_create(backend->activity, file->path);
            if (file_header_view) {
                g_tgui_view_set_position(file_header_view, x_margin, y_pos, screen_width - 2 * x_margin, file_header_height);
                g_tgui_view_set_text_size(file_header_view, 18); // Larger font for file headers
                g_tgui_view_set_text_color(file_header_view, 0xFF4444FF); // Blueish
                g_tgui_view_set_id(file_header_view, i * 10000); // Unique ID for file header (file_index * 10000)
            }
            log_debug("Rendering file: %s (Fallback)", file->path);
        }
        y_pos += file_header_height + 10; // Spacing

        for (size_t j = 0; j < file->hunk_count; j++) {
            const DiffHunk* hunk = &file->hunks[j];
            if (hunk->header) {
                // Create a Button for the hunk header (collapsible)
                void* hunk_btn = g_tgui_button_create(backend->activity, hunk->header);
                if (hunk_btn) {
                     g_tgui_view_set_position(hunk_btn, x_margin + 10, y_pos, screen_width - 2 * (x_margin + 10), hunk_header_height);
                     g_tgui_view_set_text_size(hunk_btn, 14);
                     g_tgui_view_set_text_color(hunk_btn, 0xFF000000); // Black
                     // Assign an ID for event handling: file_index * 10000 + hunk_index
                     g_tgui_view_set_id(hunk_btn, i * 10000 + j);
                }
                log_debug("  Rendering hunk: %s (Fallback)", hunk->header);
            }
            y_pos += hunk_header_height + 5;

            // Render lines if not collapsed (simplified)
            // In a real implementation, you would check hunk->is_collapsed
            // For now, let's assume all hunks are expanded for fallback rendering
            // A more sophisticated approach would involve maintaining state
            // in the backend or passing collapse state.
            for (size_t k = 0; k < hunk->line_count; k++) {
                const DiffLine* line = &hunk->lines[k];
                if (line->content) {
                    // Create a TextView for the line content
                    void* line_view = g_tgui_textview_create(backend->activity, line->content);
                    if (line_view) {
                        g_tgui_view_set_position(line_view, x_margin + 20, y_pos, screen_width - 2 * (x_margin + 20), line_height);
                        g_tgui_view_set_text_size(line_view, 12);

                        // Set color based on line type
                        uint32_t color = 0xFF000000; // Default black
                        if (line->type == LINE_TYPE_ADD) {
                            color = 0xFF00AA00; // Green
                        } else if (line->type == LINE_TYPE_DELETE) {
                            color = 0xFFAA0000; // Red
                        } else if (line->type == LINE_TYPE_CONTEXT) {
                            color = 0xFF888888; // Gray
                        }
                        g_tgui_view_set_text_color(line_view, color);
                        // Assign an ID for potential future interaction
                        // g_tgui_view_set_id(line_view, i * 1000000 + j * 1000 + k); // More complex ID scheme
                    }
                    log_debug("    Line: %s (Fallback)", line->content);
                    y_pos += line_height + 2;
                }
            }
        }
        y_pos += 20; // Extra spacing between files
    }
    log_info("Diff rendered using Termux GUI backend (Fallback)");
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ РЕНДЕРИНГА ---

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ ОБРАБОТКИ СОБЫТИЙ ---
void termux_gui_backend_handle_events(TermuxGUIBackend* backend) {
     if (!backend || !backend->initialized || !backend->running) {
        return;
    }
    // This is a simplified non-blocking check for demonstration.
    // A real implementation would have a dedicated thread or integrate into the main loop.
    // For now, we'll just log that event handling is active.
    log_debug("Handling events for Termux GUI backend (Fallback)");

    // Example of how it might look in a loop:
    /*
    while (backend->running) { // Use backend->running flag to control loop
        void* event = g_tgui_get_event(backend->conn); // Blocking or non-blocking wait
        if (!event) continue;

        int type = g_tgui_event_get_type(event);
        if (type == TGUI_EVENT_TYPE_CLICK) { // Assuming TGUI_EVENT_TYPE_CLICK is defined
            void* view = g_tgui_event_get_view(event);
            int id = g_tgui_view_get_id(view);

            // Handle click based on ID
            int file_index = id / 10000;
            int hunk_index = id % 10000;

            // Example: Toggle hunk collapse state
            // This would require access to the DiffData to modify is_collapsed flags
            // and trigger a redraw (e.g., by calling ui_manager->needs_redraw = 1;
            // and ui_manager_update_layout). This is complex and requires tight
            // integration with the app/ui_manager layer.
            // For this backend, we can only log the event.
            log_info("Clicked on view ID: %d (File: %d, Hunk: %d)", id, file_index, hunk_index);

            // Notify the application layer about the click
            // This could be done via a callback mechanism or by setting a flag
            // that the main application loop checks.
            // app_notify_hunk_click(file_index, hunk_index); // Hypothetical function

        } else if (type == TGUI_EVENT_TYPE_SCROLL) { // Assuming TGUI_EVENT_TYPE_SCROLL is defined
            // Update scroll state
            // This would involve getting scroll delta from the event
            // and updating backend's internal scroll state or notifying the app.
            // log_debug("Scroll event received");
        }
        // Add more event types as needed (e.g., TGUI_EVENT_TYPE_DESTROY for activity lifecycle)

        g_tgui_free_event(event);
        // Break condition or timeout needed in a real loop, possibly checking backend->running
        // or a timeout mechanism in g_tgui_wait_for_events.
    }
    */
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ ОБРАБОТКИ СОБЫТИЙ ---
