// src/gui/ui_manager_input.c
#include "see_code/gui/ui_manager.h"
#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>

int ui_manager_handle_touch(UIManager* ui_manager, float x, float y) {
    if (!ui_manager || !ui_manager->diff_data) {
        return 0;
    }
    log_debug("UI Manager handling touch at (%.2f, %.2f)", x, y);

    float local_y = y + ui_manager->scroll_y;
    const float line_height = LINE_HEIGHT;
    const float file_header_height = line_height;
    const float hunk_header_height = line_height;
    const float margin = MARGIN;
    const float hunk_padding = HUNK_PADDING;

    float current_y = 0.0f;
    for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
        DiffFile* file = &ui_manager->diff_data->files[i];
        if (!file->path) continue;

        float file_y_start = current_y;
        current_y += file_header_height;

        if (local_y >= file_y_start && local_y < file_y_start + file_header_height) {
            file->is_collapsed = !file->is_collapsed;
            log_info("Toggled file '%s' collapse state to %s", file->path, file->is_collapsed ? "collapsed" : "expanded");
            ui_manager->needs_redraw = 1;
            ui_manager_update_layout(ui_manager, ui_manager->scroll_y);
            return 1;
        }

        if (!file->is_collapsed) {
            for (size_t j = 0; j < file->hunk_count; j++) {
                DiffHunk* hunk = &file->hunks[j];
                if (!hunk->header) continue;

                float hunk_y_start = current_y;
                current_y += hunk_header_height;

                if (local_y >= hunk_y_start && local_y < hunk_y_start + hunk_header_height) {
                    hunk->is_collapsed = !hunk->is_collapsed;
                    log_info("Toggled hunk '%.50s...' collapse state to %s", hunk->header, hunk->is_collapsed ? "collapsed" : "expanded");
                    ui_manager->needs_redraw = 1;
                    ui_manager_update_layout(ui_manager, ui_manager->scroll_y);
                    return 1;
                }

                if (!hunk->is_collapsed) {
                    current_y += hunk->line_count * line_height;
                }
                current_y += 5.0f;
            }
        }
        current_y += 10.0f;
    }

    log_debug("Touch did not hit a collapsible header");
    return 1;
}
