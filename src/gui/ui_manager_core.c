// src/gui/ui_manager_core.c
#include "see_code/gui/ui_manager.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>

struct UIManager {
    Renderer* renderer;
    TermuxGUIBackend* termux_backend;
    DiffData* diff_data;
    float scroll_y;
    float content_height;
    RendererType active_renderer;
    int needs_redraw;
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
    ui_manager->active_renderer = RENDERER_TYPE_GLES2;

    // Try to create backend Termux-GUI for potential fallback
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
    ui_manager->needs_redraw = 1;
    ui_manager_update_layout(ui_manager, ui_manager->scroll_y);
}

void ui_manager_update_layout(UIManager* ui_manager, float scroll_y) {
    if (!ui_manager) {
        return;
    }
    ui_manager->scroll_y = scroll_y;
    float total_height = 0.0f;
    const float line_height = LINE_HEIGHT;
    const float hunk_header_height = line_height;
    const float file_header_height = line_height;
    const float margin = MARGIN;
    const float hunk_padding = HUNK_PADDING;
    const float file_margin = 10.0f;
    const float hunk_margin = 5.0f;

    if (ui_manager->diff_data) {
        for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
            DiffFile* file = &ui_manager->diff_data->files[i];
            total_height += file_header_height + file_margin;

            if (!file->is_collapsed) {
                for (size_t j = 0; j < file->hunk_count; j++) {
                    DiffHunk* hunk = &file->hunks[j];
                    total_height += hunk_header_height + hunk_margin;

                    if (!hunk->is_collapsed) {
                        total_height += hunk->line_count * line_height;
                    }
                    total_height += 5.0f;
                }
            }
            total_height += 10.0f;
        }
    }
    ui_manager->content_height = total_height;
    log_debug("Updated layout: scroll_y=%.2f, content_height=%.2f", ui_manager->scroll_y, ui_manager->content_height);
}

float ui_manager_get_content_height(UIManager* ui_manager) {
    if (!ui_manager) {
        return 0.0f;
    }
    return ui_manager->content_height;
}

void ui_manager_set_renderer_type(UIManager* ui_manager, RendererType type) {
    if (!ui_manager) return;

    if (type == RENDERER_TYPE_TERMUX_GUI && !ui_manager->termux_backend) {
        log_warn("Cannot switch to Termux GUI renderer: backend not available");
        return;
    }

    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && type == RENDERER_TYPE_TERMUX_GUI) {
        log_info("Switching renderer from GLES2 to Termux GUI (Fallback)");
    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && type == RENDERER_TYPE_GLES2) {
        log_info("Switching renderer from Termux GUI (Fallback) to GLES2");
    }

    ui_manager->active_renderer = type;
    ui_manager->needs_redraw = 1;
}

RendererType ui_manager_get_renderer_type(const UIManager* ui_manager) {
    if (!ui_manager) return RENDERER_TYPE_GLES2;
    return ui_manager->active_renderer;
}
