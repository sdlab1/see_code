// src/gui/ui_manager_render.c
// УЛУЧШЕНИЯ:
// 1. Цвета передаются напрямую (uint32_t), а не вычисляются каждый кадр.
// 2. Обрезка строк убрана, т.к. теперь выполняется в text_renderer.
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <string.h>

void ui_manager_render(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && ui_manager->renderer) {
        if (!ui_manager->diff_data) return;

        float y_pos = -ui_manager->scroll_y;
        const float screen_height = (float)renderer_get_height(ui_manager->renderer);
        const float screen_width = (float)renderer_get_width(ui_manager->renderer);

        for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
            DiffFile* file = &ui_manager->diff_data->files[i];
            if (!file->path) continue;

            // Оценка высоты для отсечения (culling)
            float file_height_estimate = FILE_HEADER_HEIGHT + 10.0f;
            if (!file->is_collapsed) {
                for (size_t j = 0; j < file->hunk_count; j++) {
                    file_height_estimate += HUNK_HEADER_HEIGHT + 5.0f;
                    if (!file->hunks[j].is_collapsed) {
                        file_height_estimate += file->hunks[j].line_count * LINE_HEIGHT;
                    }
                }
            }
            
            if (y_pos + file_height_estimate < 0) {
                y_pos += file_height_estimate;
                continue;
            }
            if (y_pos > screen_height) {
                break;
            }
            
            // Отрисовка заголовка файла
            renderer_draw_quad(ui_manager->renderer, MARGIN, y_pos, screen_width - 2 * MARGIN, FILE_HEADER_HEIGHT, COLOR_FILE_HEADER);
            renderer_draw_text(ui_manager->renderer, file->path, MARGIN + 5.0f, y_pos + 22.0f, 1.0f, 0xFFFFFFFF, screen_width - 2 * MARGIN - 10.0f);
            y_pos += FILE_HEADER_HEIGHT + 10.0f;

            if (!file->is_collapsed) {
                for (size_t j = 0; j < file->hunk_count; j++) {
                    DiffHunk* hunk = &file->hunks[j];
                    if (!hunk->header) continue;

                    renderer_draw_quad(ui_manager->renderer, MARGIN, y_pos, screen_width - 2 * MARGIN, HUNK_HEADER_HEIGHT, COLOR_HUNK_HEADER);
                    renderer_draw_text(ui_manager->renderer, hunk->header, MARGIN + 5.0f, y_pos + 22.0f, 1.0f, 0xFF000000, screen_width - 2 * MARGIN - 10.0f);
                    y_pos += HUNK_HEADER_HEIGHT + 5.0f;

                    if (!hunk->is_collapsed) {
                        for (size_t k = 0; k < hunk->line_count; k++) {
                            DiffLine* line = &hunk->lines[k];
                            if (!line->content) continue;

                            uint32_t bg_color;
                            switch(line->type) {
                                case LINE_TYPE_ADD:    bg_color = COLOR_ADD_LINE; break;
                                case LINE_TYPE_DELETE: bg_color = COLOR_DEL_LINE; break;
                                default:               bg_color = COLOR_CONTEXT_LINE; break;
                            }
                            
                            renderer_draw_quad(ui_manager->renderer, MARGIN + HUNK_PADDING, y_pos, screen_width - 2 * (MARGIN + HUNK_PADDING), LINE_HEIGHT, bg_color);
                            renderer_draw_text(ui_manager->renderer, line->content, MARGIN + HUNK_PADDING + 5.0f, y_pos + 16.0f, 1.0f, 0xFF000000, screen_width - 2 * (MARGIN + HUNK_PADDING) - 10.0f);
                            
                            y_pos += LINE_HEIGHT;
                        }
                    }
                }
            }
        }
    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && ui_manager->termux_backend) {
        if (ui_manager->diff_data) {
            termux_gui_backend_render_diff(ui_manager->termux_backend, ui_manager->diff_data);
        }
    }
}
