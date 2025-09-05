// src/data/diff_data.c
#include "see_code/data/diff_data.h"
#include "see_code/data/diff_parser.h" // Подключаем новый парсер
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>

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

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ ЗАГРУЗКИ ---
// Теперь она использует новый парсер
int diff_data_load_from_buffer(DiffData* data, const char* buffer, size_t buffer_size) {
    if (!data || !buffer || buffer_size == 0) {
        log_error("Invalid arguments to diff_data_load_from_buffer");
        return 0;
    }

    // Очищаем существующие данные
    diff_data_clear(data);

    // Делегируем парсинг новому модулю
    int result = diff_parser_parse(data, buffer, buffer_size);

    // Логирование результата делегируется внутрь diff_parser_parse
    // if (result) {
    //     log_info("Successfully loaded diff data with %zu files", data->file_count);
    // } else {
    //     log_error("Failed to load diff data from buffer");
    // }

    return result;
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ ЗАГРУЗКИ ---

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
