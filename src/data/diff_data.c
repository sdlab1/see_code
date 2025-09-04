// src/data/diff_data.c
#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // For sscanf

DiffData* diff_data_create(void) {
    DiffData* data = malloc(sizeof(DiffData));
    if (!data) {
        log_error("Failed to allocate memory for DiffData");
        return NULL;
    }
    memset(data, 0, sizeof(DiffData));
    return data;
}

void diff_data_destroy(DiffData* data) {
    if (!data) {
        return;
    }
    diff_data_clear(data);
    free(data);
}

// --- ИСПРАВЛЕННАЯ ФУНКЦИЯ ---
int diff_data_load_from_buffer(DiffData* data, const char* buffer, size_t buffer_size) {
    if (!data || !buffer || buffer_size == 0) {
        log_error("Invalid arguments to diff_data_load_from_buffer");
        return 0;
    }

    diff_data_clear(data);

    char* buffer_copy = malloc(buffer_size + 1);
    if (!buffer_copy) {
        log_error("Failed to allocate memory for buffer copy");
        return 0;
    }
    memcpy(buffer_copy, buffer, buffer_size);
    buffer_copy[buffer_size] = '\0';

    char *saveptr = NULL;
    char *line = strtok_r(buffer_copy, "\n", &saveptr);

    DiffFile* current_file = NULL;
    DiffHunk* current_hunk = NULL;

    while (line != NULL) {
        char *path_a, *path_b;
        // Новый файл
        if (sscanf(line, "diff --git a/%ms b/%ms", &path_a, &path_b) == 2) {
            // Если был предыдущий файл с ханком, убедимся, что он учтен
            if (current_file && current_hunk && current_hunk->header) {
                // current_hunk уже находится в current_file->hunks[current_file->hunk_count-1]
                // Ничего дополнительно делать не нужно, кроме как сбросить current_hunk
            }

            // Добавляем новый файл
            if (data->file_count >= data->file_capacity) {
                size_t new_capacity = (data->file_capacity == 0) ? 1 : data->file_capacity * 2;
                DiffFile* tmp = realloc(data->files, new_capacity * sizeof(DiffFile));
                if (!tmp) {
                    log_error("Failed to reallocate files array");
                    free(path_a);
                    if (path_b) free(path_b);
                    free(buffer_copy);
                    diff_data_clear(data);
                    return 0;
                }
                // Инициализируем новую память нулями
                memset(tmp + data->file_capacity, 0, (new_capacity - data->file_capacity) * sizeof(DiffFile));
                data->files = tmp;
                data->file_capacity = new_capacity;
            }

            current_file = &data->files[data->file_count];
            current_file->path = strdup(path_b ? path_b : path_a);
            current_file->path_length = current_file->path ? strlen(current_file->path) : 0;
            // Инициализируем массив ханков для нового файла
            current_file->hunk_capacity = 1;
            current_file->hunks = calloc(current_file->hunk_capacity, sizeof(DiffHunk));
            if (!current_file->hunks && current_file->hunk_capacity > 0) {
                 log_error("Failed to allocate initial hunks for file %s", current_file->path ? current_file->path : "unknown");
                 free(path_a);
                 if (path_b) free(path_b);
                 free(buffer_copy);
                 diff_data_clear(data);
                 return 0;
            }
            data->file_count++;
            current_hunk = NULL; // Сброс ханка для нового файла

            free(path_a);
            if (path_b) free(path_b);
        }
        // Новый ханк (только если файл уже начат)
        else if (current_file && strncmp(line, "@@", 2) == 0) {
             // Добавляем новый ханк в текущий файл
            if (current_file->hunk_count >= current_file->hunk_capacity) {
                size_t new_capacity = current_file->hunk_capacity * 2;
                DiffHunk* tmp = realloc(current_file->hunks, new_capacity * sizeof(DiffHunk));
                if (!tmp) {
                    log_error("Failed to reallocate hunks for file %s", current_file->path ? current_file->path : "unknown");
                    free(buffer_copy);
                    diff_data_clear(data);
                    return 0;
                }
                // Инициализируем новую память нулями
                memset(tmp + current_file->hunk_capacity, 0, (new_capacity - current_file->hunk_capacity) * sizeof(DiffHunk));
                current_file->hunks = tmp;
                current_file->hunk_capacity = new_capacity;
            }

            current_hunk = &current_file->hunks[current_file->hunk_count];
            current_hunk->header = strdup(line);
            current_hunk->header_length = current_hunk->header ? strlen(current_hunk->header) : 0;
            // Инициализируем массив строк для нового ханка
            current_hunk->line_capacity = 10;
            current_hunk->lines = calloc(current_hunk->line_capacity, sizeof(DiffLine));
            if (!current_hunk->lines && current_hunk->line_capacity > 0) {
                 log_error("Failed to allocate initial lines for hunk in file %s", current_file->path ? current_file->path : "unknown");
                 free(buffer_copy);
                 diff_data_clear(data);
                 return 0;
            }
            current_hunk->line_count = 0;
            current_file->hunk_count++; // Увеличиваем счетчик ханков в файле
        }
        // Строка контента ханка (только если ханк начат)
        else if (current_hunk && (line[0] == ' ' || line[0] == '+' || line[0] == '-')) {
            // Добавляем строку в текущий ханк
            if (current_hunk->line_count >= current_hunk->line_capacity) {
                size_t new_capacity = current_hunk->line_capacity * 2;
                DiffLine* tmp = realloc(current_hunk->lines, new_capacity * sizeof(DiffLine));
                if (!tmp) {
                    log_error("Failed to reallocate lines for hunk in file %s", current_file->path ? current_file->path : "unknown");
                    free(buffer_copy);
                    diff_data_clear(data);
                    return 0;
                }
                // Инициализируем новую память нулями
                memset(tmp + current_hunk->line_capacity, 0, (new_capacity - current_hunk->line_capacity) * sizeof(DiffLine));
                current_hunk->lines = tmp;
                current_hunk->line_capacity = new_capacity;
            }

            DiffLine* new_line = &current_hunk->lines[current_hunk->line_count];
            new_line->content = strdup(line);
            new_line->length = new_line->content ? strlen(new_line->content) : 0;
            switch(line[0]) {
                case '+': new_line->type = LINE_TYPE_ADD; break;
                case '-': new_line->type = LINE_TYPE_DELETE; break;
                default: new_line->type = LINE_TYPE_CONTEXT; break;
            }
            current_hunk->line_count++;
        }
        // Иначе игнорируем строку

        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(buffer_copy);
    log_info("Successfully parsed diff data with %zu files", data->file_count);
    return 1;
}
// --- КОНЕЦ ИСПРАВЛЕННОЙ ФУНКЦИИ ---

void diff_data_clear(DiffData* data) {
    if (!data) {
        return;
    }
    for (size_t i = 0; i < data->file_count; i++) {
        DiffFile* file = &data->files[i];
        free(file->path);
        for (size_t j = 0; j < file->hunk_count; j++) {
            DiffHunk* hunk = &file->hunks[j];
            free(hunk->header);
            for (size_t k = 0; k < hunk->line_count; k++) {
                free(hunk->lines[k].content);
            }
            free(hunk->lines);
        }
        free(file->hunks);
    }
    free(data->files);
    // Важно: обнуляем все поля структуры
    memset(data, 0, sizeof(DiffData));
}
