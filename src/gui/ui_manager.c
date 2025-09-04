#include "see_code/gui/ui_manager.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

struct UIManager {
    Renderer* renderer;
    DiffData* diff_data;
    float scroll_y;
    float content_height;
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
    
    return ui_manager;
}

void ui_manager_destroy(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
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
    if (!ui_manager || !ui_manager->renderer) {
        return;
    }
    
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
}

int ui_manager_handle_touch(UIManager* ui_manager, float x, float y) {
    if (!ui_manager) {
        return 0;
    }
    
    log_debug("UI Manager handling touch at (%.2f, %.2f)", x, y);
    
    // Placeholder touch handling
    // In a real implementation, you would check if touch is on a specific UI element
    
    return 1; // Touch handled
}

float ui_manager_get_content_height(UIManager* ui_manager) {
    if (!ui_manager) {
        return 0.0f;
    }
    
    return ui_manager->content_height;
}
