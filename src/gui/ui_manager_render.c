// src/gui/ui_manager_render.c
#include "see_code/gui/ui_manager.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/termux_gui_backend.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdlib.h>
#include <string.h>

void ui_manager_render(UIManager* ui_manager) {
    if (!ui_manager) {
        return;
    }

    if (ui_manager->active_renderer == RENDERER_TYPE_GLES2 && ui_manager->renderer) {
        log_debug("Rendering with GLES2");

        renderer_clear(
            ((COLOR_BACKGROUND >> 16) & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 8) & 0xFF) / 255.0f,
            (COLOR_BACKGROUND & 0xFF) / 255.0f,
            ((COLOR_BACKGROUND >> 24) & 0xFF) / 255.0f
        );

        if (ui_manager->diff_data) {
            float y_pos = -ui_manager->scroll_y;
            const float line_height = LINE_HEIGHT;
            const float hunk_header_height = line_height;
            const float file_header_height = line_height;
            const float margin = MARGIN;
            const float hunk_padding = HUNK_PADDING;
            const float screen_height = (float)renderer_get_height(ui_manager->renderer);
            const float screen_width = (float)renderer_get_width(ui_manager->renderer);

            for (size_t i = 0; i < ui_manager->diff_data->file_count; i++) {
                DiffFile* file = &ui_manager->diff_data->files[i];
                if (!file->path) continue;

                float file_y_start = y_pos;
                if (file_y_start > ui_manager->scroll_y + screen_height) {
                     log_debug("File %zu is below screen, stopping render", i);
                     break;
                }

                float file_height_estimate = file_header_height + MARGIN;
                if (!file->is_collapsed) {
                    for (size_t fj = 0; fj < file->hunk_count; fj++) {
                        DiffHunk* fhunk = &file->hunks[fj];
                        file_height_estimate += hunk_header_height + HUNK_PADDING;
                        if (!fhunk->is_collapsed) {
                            file_height_estimate += fhunk->line_count * line_height;
                        }
                        file_height_estimate += 5.0f;
                    }
                }
                file_height_estimate += 10.0f;

                if (y_pos + file_height_estimate < ui_manager->scroll_y) {
                     log_debug("File %zu is above screen, skipping", i);
                     y_pos += file_height_estimate;
                     continue;
                }

                float r_file = ((COLOR_FILE_HEADER >> 16) & 0xFF) / 255.0f;
                float g_file = ((COLOR_FILE_HEADER >> 8) & 0xFF) / 255.0f;
                float b_file = (COLOR_FILE_HEADER & 0xFF) / 255.0f;
                float a_file = ((COLOR_FILE_HEADER >> 24) & 0xFF) / 255.0f;
                renderer_draw_quad(ui_manager->renderer, margin, y_pos, screen_width - 2 * margin, file_header_height,
                                   r_file, g_file, b_file, a_file);
                // --- РИСУЕМ ТЕКСТ ЗАГОЛОВКА ФАЙЛА ---
                renderer_draw_text(ui_manager->renderer, file->path,
                                   margin + 5.0f,
                                   y_pos + (file_header_height - 16.0f) / 2.0f,
                                   1.0f,
                                   0xFFFFFFFF);
                // --- КОНЕЦ РИСОВАНИЯ ТЕКСТА ---
                y_pos += file_header_height;

                if (!file->is_collapsed) {
                    for (size_t j = 0; j < file->hunk_count; j++) {
                        DiffHunk* hunk = &file->hunks[j];
                        if (!hunk->header) continue;

                        float hunk_y_start = y_pos;
                        if (y_pos + hunk_header_height < ui_manager->scroll_y) {
                             float hunk_height = hunk_header_height + HUNK_PADDING;
                             if (!hunk->is_collapsed) {
                                 hunk_height += hunk->line_count * line_height;
                             }
                             hunk_height += 5.0f;
                             y_pos += hunk_height;
                             continue;
                        }

                        float r_hunk = ((COLOR_HUNK_HEADER >> 16) & 0xFF) / 255.0f;
                        float g_hunk = ((COLOR_HUNK_HEADER >> 8) & 0xFF) / 255.0f;
                        float b_hunk = (COLOR_HUNK_HEADER & 0xFF) / 255.0f;
                        float a_hunk = ((COLOR_HUNK_HEADER >> 24) & 0xFF) / 255.0f;
                        renderer_draw_quad(ui_manager->renderer, margin + hunk_padding, y_pos, screen_width - 2 * (margin + hunk_padding), hunk_header_height,
                                           r_hunk, g_hunk, b_hunk, a_hunk);
                        // --- РИСУЕМ ТЕКСТ ЗАГОЛОВКА ХАНКА ---
                        renderer_draw_text(ui_manager->renderer, hunk->header,
                                           margin + hunk_padding + 5.0f,
                                           y_pos + (hunk_header_height - 16.0f) / 2.0f,
                                           1.0f,
                                           0xFF000000);
                        // --- КОНЕЦ РИСОВАНИЯ ТЕКСТА ---
                        y_pos += hunk_header_height;

                        if (!hunk->is_collapsed) {
                            for (size_t k = 0; k < hunk->line_count; k++) {
                                DiffLine* line = &hunk->lines[k];
                                if (!line->content) continue;

                                if (y_pos + line_height < ui_manager->scroll_y) {
                                     y_pos += line_height;
                                     continue;
                                }
                                if (y_pos > ui_manager->scroll_y + screen_height) {
                                     y_pos += (hunk->line_count - k - 1) * line_height;
                                     break;
                                }

                                float r_line, g_line, b_line, a_line = 1.0f;
                                switch (line->type) {
                                    case LINE_TYPE_ADD:
                                        r_line = ((COLOR_ADD_LINE >> 16) & 0xFF) / 255.0f;
                                        g_line = ((COLOR_ADD_LINE >> 8) & 0xFF) / 255.0f;
                                        b_line = (COLOR_ADD_LINE & 0xFF) / 255.0f;
                                        break;
                                    case LINE_TYPE_DELETE:
                                        r_line = ((COLOR_DEL_LINE >> 16) & 0xFF) / 255.0f;
                                        g_line = ((COLOR_DEL_LINE >> 8) & 0xFF) / 255.0f;
                                        b_line = (COLOR_DEL_LINE & 0xFF) / 255.0f;
                                        break;
                                    default:
                                        r_line = ((COLOR_CONTEXT_LINE >> 16) & 0xFF) / 255.0f;
                                        g_line = ((COLOR_CONTEXT_LINE >> 8) & 0xFF) / 255.0f;
                                        b_line = (COLOR_CONTEXT_LINE & 0xFF) / 255.0f;
                                        break;
                                }
                                renderer_draw_quad(ui_manager->renderer, margin + 2 * hunk_padding, y_pos, screen_width - 2 * (margin + 2 * hunk_padding), line_height,
                                                   r_line, g_line, b_line, a_line);
                                // --- РИСУЕМ ТЕКСТ СТРОКИ ---
                                char display_buffer[105];
                                const char* display_text = line->content;
                                if (line->length > 100) {
                                    strncpy(display_buffer, line->content, 100);
                                    display_buffer[100] = '\0';
                                    strcat(display_buffer, "...");
                                    display_text = display_buffer;
                                }
                                renderer_draw_text(ui_manager->renderer, display_text,
                                                   margin + 2 * hunk_padding + 5.0f,
                                                   y_pos + (line_height - 16.0f) / 2.0f,
                                                   1.0f,
                                                   0xFF000000);
                                // --- КОНЕЦ РИСОВАНИЯ ТЕКСТА ---
                                y_pos += line_height;
                            }
                        }
                        y_pos += 5.0f;
                    }
                }
                y_pos += 10.0f;
            }
        }

    } else if (ui_manager->active_renderer == RENDERER_TYPE_TERMUX_GUI && ui_manager->termux_backend) {
        log_debug("Rendering with Termux GUI (Fallback)");

        if (ui_manager->diff_data) {
            termux_gui_backend_render_diff(ui_manager->termux_backend, ui_manager->diff_data);
        }
    } else {
        log_error("No valid renderer available for ui_manager_render");
    }
}
