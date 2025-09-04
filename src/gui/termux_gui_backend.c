// src/gui/termux_gui_backend.c
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

// Dynamically loaded function pointers for termux-gui-c
static void* g_termux_gui_lib = NULL;

// Function pointer types
typedef void* (*tgui_create_t)(void);
typedef void (*tgui_destroy_t)(void*);
typedef void* (*tgui_create_activity_t)(void*, int);
// Add more as needed...

// Global function pointers
static tgui_create_t g_tgui_create = NULL;
static tgui_destroy_t g_tgui_destroy = NULL;
static tgui_create_activity_t g_tgui_create_activity = NULL;
// ... more ...

struct TermuxGUIBackend {
    void* conn; // tgui_connection*
    void* activity; // tgui_activity*
    int initialized;
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
    g_tgui_create = (tgui_create_t) dlsym(g_termux_gui_lib, "tgui_create");
    g_tgui_destroy = (tgui_destroy_t) dlsym(g_termux_gui_lib, "tgui_destroy");
    g_tgui_create_activity = (tgui_create_activity_t) dlsym(g_termux_gui_lib, "tgui_create_activity");
    
    if (!g_tgui_create || !g_tgui_destroy || !g_tgui_create_activity) {
        log_error("Failed to load essential termux-gui-c symbols");
        dlclose(g_termux_gui_lib);
        g_termux_gui_lib = NULL;
        g_tgui_create = NULL;
        g_tgui_destroy = NULL;
        g_tgui_create_activity = NULL;
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
        g_tgui_create = NULL;
        g_tgui_destroy = NULL;
        g_tgui_create_activity = NULL;
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

    backend->conn = g_tgui_create();
    if (!backend->conn) {
        log_error("Failed to create termux-gui connection");
        return 0;
    }

    backend->activity = g_tgui_create_activity(backend->conn, 0);
    if (!backend->activity) {
        log_error("Failed to create termux-gui activity");
        g_tgui_destroy(backend->conn);
        backend->conn = NULL;
        return 0;
    }

    // Set orientation, etc.
    // tgui_set_activity_orientation(backend->activity, 1); // Example

    backend->initialized = 1;
    log_info("TermuxGUIBackend initialized successfully");
    return 1;
}

void termux_gui_backend_render_diff(TermuxGUIBackend* backend, const DiffData* data) {
    if (!backend || !backend->initialized || !data) {
        return;
    }

    // Clear previous views
    // tgui_clear_views(backend->activity);

    // Iterate through DiffData and create TextViews/Buttons using termux-gui-c API
    // This is a simplified example structure
    float y_pos = 50.0f;
    for (size_t i = 0; i < data->file_count; i++) {
        const DiffFile* file = &data->files[i];
        if (file->path) {
            // Create a TextView for the file path
            // tgui_view* file_header = tgui_create_textview(backend->activity, file->path);
            // tgui_set_view_position(file_header, 10, (int)y_pos, 1000, 40);
            // tgui_set_textview_textsize(file_header, HEADER_FONT_SIZE);
            log_debug("Rendering file: %s (Fallback)", file->path); // Placeholder
        }
        y_pos += 60.0f; // Spacing

        for (size_t j = 0; j < file->hunk_count; j++) {
            const DiffHunk* hunk = &file->hunks[j];
            if (hunk->header) {
                // Create a Button for the hunk header (collapsible)
                // tgui_view* hunk_btn = tgui_create_button(backend->activity, hunk->header);
                // tgui_set_view_position(hunk_btn, 20, (int)y_pos, 1000, 35);
                // Assign an ID for event handling: i * 10000 + j
                // tgui_set_view_id(hunk_btn, i * 10000 + j);
                log_debug("  Rendering hunk: %s (Fallback)", hunk->header); // Placeholder
            }
            y_pos += 50.0f;

            // Render lines if not collapsed (simplified)
            for (size_t k = 0; k < hunk->line_count; k++) {
                const DiffLine* line = &hunk->lines[k];
                if (line->content) {
                    // tgui_view* line_view = tgui_create_textview(backend->activity, line->content);
                    // tgui_set_view_position(line_view, 40, (int)y_pos, 1000, LINE_HEIGHT);
                    // Set color based on line type
                    /*
                    if (line->type == LINE_TYPE_ADD) {
                        tgui_set_textview_textcolor(line_view, COLOR_ADD_LINE);
                    } else if (line->type == LINE_TYPE_DELETE) {
                        tgui_set_textview_textcolor(line_view, COLOR_DEL_LINE);
                    } else {
                        tgui_set_textview_textcolor(line_view, COLOR_CONTEXT_LINE);
                    }
                    */
                    log_debug("    Line: %s (Fallback)", line->content); // Placeholder
                    y_pos += LINE_HEIGHT + 2;
                }
            }
        }
    }
    log_info("Diff rendered using Termux GUI backend (Fallback)");
}

void termux_gui_backend_handle_events(TermuxGUIBackend* backend) {
     if (!backend || !backend->initialized) {
        return;
    }
    // In a real implementation, you would have a loop here calling tgui_wait_event
    // and handling click/scroll events to collapse/expand hunks and scroll.
    // For this example, we'll just log that event handling is active.
    log_debug("Handling events for Termux GUI backend (Fallback)");
    // Example:
    /*
    while (1) {
        tgui_event* event = tgui_wait_event(backend->conn);
        if (!event) continue;
        if (event->type == TGUI_EVENT_CLICK) {
            // handle_button_click(event->view_id);
        } else if (event->type == TGUI_EVENT_SCROLL) {
            // Update scroll state
        }
        tgui_free_event(event);
        // Break condition or timeout needed in a real loop
    }
    */
}
