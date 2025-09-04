// src/gui/ui_manager.c
#include "see_code/gui/ui_manager.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

struct UIManager {
    Renderer* renderer; // GLES2 renderer
    TermuxGUIBackend* termux_backend; // Fallback renderer
    DiffData* diff_data;
    float scroll_y;
    float content_height;
    RendererType active_renderer; // Track which renderer is active
};

UIManager* ui_manager_create(Renderer* renderer) {
    if (!renderer) {
        log_error("Invalid renderer provided to ui_manager_create");
        return NULL;
    }

    UIManager* ui_manager = malloc(sizeof(UIManager));
    if (!ui_manager) {
        log_error("Failed to allocate memory for UIManager");
        return NULL;
    }
    memset(ui_manager, 0, sizeof(UIManager));

    ui_manager->renderer = renderer;
    ui_manager->active_renderer = RENDERER_TYPE_GLES2; // Default to GLES2
    
    // Try to create the Termux GUI backend for potential fallback
    if (termux_gui_backend_is_available()) {
        ui_manager->termux_backend = termux_gui_backend_create();
        if (ui_manager->termux_backend) {
             if (!termux_gui_backend_init(ui_manager->termux_backend)) {
                 log_warn("Failed to initialize Termux GUI backend, fallback will not be available");
                 termux_gui_backend_destroy(ui_manager->termux_backend);
                 ui_manager->termux_backend = NULL;
             } else {
                 log_info("Termux GUI backend created and initialized for potential fallback");
             }
        }
    } else {
        log_info("Termux GUI backend not available on this system");
    }

    return ui_manager;
}

void ui_manager_destroy(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }
    if (ui_manager->termux_backend) {
        termux_gui_backend_destroy(ui_manager->termux_backend);
    }
    free(ui_manager);
}

void ui_manager_set_diff_data(UIManager* ui_manager, DiffData* data) {
    if (!ui_manager) {
        return;
    }
    ui_manager->diff_data = data;
}

void ui_manager_update_layout(UIManager* ui_manager, float scroll_y) {
    if (!ui_manager) {
        return;
    }
    ui_manager->scroll_y = scroll_y;
    // Calculate content height based on diff data
    if (ui_manager->diff_data) {
        // Simple calculation - in a real implementation, you would measure text
        ui_manager->content_height = ui_manager->diff_data->file_count * 100.0f; // Placeholder
    } else {
        ui_manager->content_height = 0.0f;
    }
}

void ui_manager_render(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && ui_manager->renderer) {
        // --- GLES2 Rendering Path ---
        log_debug("Rendering with GLES2");
        
        // Clear screen
        renderer_clear(
            ((COLOR_BACKGROUND >> 16) & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 8) & 0xFF) / 255.0f,
            (COLOR_BACKGROUND & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 24) & 0xFF) / 255.0f
        );

        // Render diff data if available
        if (ui_manager->diff_data) {
            // Placeholder rendering - in a real implementation, you would render text
            for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
                float y_pos = i * 100.0f - ui_manager->scroll_y;
                // Only render if visible
                if (y_pos > -100.0f && y_pos < 2400.0f) { // Assuming 2400px screen height
                    // Draw file header background
                    renderer_draw_quad(
                        0, y_pos, 1080, 50,
                        ((COLOR_FILE_HEADER >> 16) & 0xFF) / 255.0f,
                        ((COLOR_FILE_HEADER >> 8) & 0xFF) / 255.0f,
                        (COLOR_FILE_HEADER & 0xFF) / 255.0f,
                        ((COLOR_FILE_HEADER >> 24) & 0xFF) / 255.0f
                    );
                    // Draw file path text (placeholder)
                    // In a real implementation, you would use FreeType/TrueType to render text
                }
            }
        }
        
    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && ui_manager->termux_backend) {
        // --- Termux GUI Fallback Rendering Path ---
        log_debug("Rendering with Termux GUI (Fallback)");
        
        if (ui_manager->diff_data) {
            termux_gui_backend_render_diff(ui_manager->termux_backend, ui_manager->diff_data);
            // In a real app, you'd also need a way to process events from the Termux GUI
            // This might require a separate thread or integration into the main loop
            // termux_gui_backend_handle_events(ui_manager->termux_backend); 
        }
    } else {
        log_error("No valid renderer available for ui_manager_render");
    }
}

int ui_manager_handle_touch(UIManager* ui_manager, float x, float y) {
    if (!ui_manager) {
        return 0;
    }
    log_debug("UI Manager handling touch at (%.2f, %.2f)", x, y);
    
    if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI) {
        // Forward touch events to Termux GUI backend if it's handling UI
        // termux_gui_backend_handle_events(ui_manager->termux_backend); // Or specific touch handler
        log_debug("Touch event forwarded to Termux GUI backend");
        return 1; // Assume handled
    }
    
    // Placeholder touch handling for GLES2
    // In a real implementation, you would check if touch is on a specific UI element
    return 1; // Touch handled
}

float ui_manager_get_content_height(UIManager* ui_manager) {
    if (!ui_manager) {
        return 0.0f;
    }
    return ui_manager->content_height;
}

// --- NEW FUNCTIONS IMPLEMENTATION ---
void ui_manager_set_renderer_type(UIManager* ui_manager, RendererType type) {
    if (!ui_manager) return;
    
    if (type == RENDERER_TYPE_TERMUX_GUI && !ui_manager->termux_backend) {
        log_warn("Cannot switch to Termux GUI renderer: backend not available");
        return; // Cannot switch if backend is not available
    }
    
    // Perform any necessary cleanup/setup when switching renderers
    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && type == RENDERER_TYPE_TERMUX_GUI) {
        log_info("Switching renderer from GLES2 to Termux GUI (Fallback)");
        // Cleanup GLES2 state if needed (e.g., textures)
    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && type == RENDERER_TYPE_GLES2) {
        log_info("Switching renderer from Termux GUI (Fallback) to GLES2");
        // Cleanup Termux GUI state if needed
    }
    
    ui_manager->active_renderer = type;
}

RendererType ui_manager_get_renderer_type(const UIManager* ui_manager) {
    if (!ui_manager) return RENDERER_TYPE_GLES2; // Default/fail-safe
    return ui_manager->active_renderer;
}
