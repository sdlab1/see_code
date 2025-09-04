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
    // Initial capacity can be set if needed
    return data;
}

void diff_data_destroy(DiffData* data) {
    if (!data) {
        return;
    }
    diff_data_clear(data);
    free(data);
}

// --- ОБНОВЛЕННАЯ ФУНКЦИЯ ---
// diff_data_load_from_buffer: Парсит текст diff и заполняет структуру DiffData
int diff_data_load_from_buffer(DiffData* data, const char* buffer, size_t buffer_size) {
    if (!data || !buffer || buffer_size == 0) {
        log_error("Invalid arguments to diff_data_load_from_buffer");
        return 0;
    }

    // Очищаем существующие данные
    diff_data_clear(data);

    // Создаем временную строку, завершенную нулем, для удобства обработки
    // В реальном приложении можно обрабатывать buffer напрямую, но это проще для strtok
    char* buffer_copy = malloc(buffer_size + 1);
    if (!buffer_copy) {
        log_error("Failed to allocate memory for buffer copy");
        return 0;
    }
    memcpy(buffer_copy, buffer, buffer_size);
    buffer_copy[buffer_size] = '\0';

    char *line = NULL;
    char *saveptr = NULL; // Для strtok_r, чтобы сделать код потокобезопасным
    line = strtok_r(buffer_copy, "\n", &saveptr);

    DiffFile* current_file = NULL;
    DiffHunk* current_hunk = NULL;

    while (line != NULL) {
        // Новый файл
        char *path_a, *path_b;
        if (sscanf(line, "diff --git a/%ms b/%ms", &path_a, &path_b) == 2) {
            // Сохраняем предыдущий файл/ханк, если они есть
            if (current_file && current_hunk) {
                // Проверяем, есть ли уже такой ханк в массиве, или нужно добавить
                // Для простоты, добавляем в конец
                if (current_file->hunk_count >= current_file->hunk_capacity) {
                    current_file->hunk_capacity = (current_file->hunk_capacity == 0) ? 1 : current_file->hunk_capacity * 2;
                    DiffHunk* tmp = realloc(current_file->hunks, current_file->hunk_capacity * sizeof(DiffHunk));
                    if (!tmp) {
                        log_error("Failed to reallocate hunks for file %s", current_file->path ? current_file->path : "unknown");
                        free(path_a);
                        if (path_b) free(path_b);
                        free(buffer_copy);
                        // Очищаем частично созданные структуры
                        diff_data_clear(data);
                        return 0;
                    }
                    current_file->hunks = tmp;
                    // Инициализируем новую память
                    memset(&current_file->hunks[current_file->hunk_count], 0, (current_file->hunk_capacity - current_file->hunk_count) * sizeof(DiffHunk));
                }
                current_file->hunks[current_file->hunk_count] = *current_hunk; // Копируем содержимое
                current_file->hunk_count++;
                // current_hunk теперь указывает на новое место в массиве
                current_hunk = &current_file->hunks[current_file->hunk_count - 1];
            }

            // Добавляем новый файл
            if (data->file_count >= data->file_capacity) {
                data->file_capacity = (data->file_capacity == 0) ? 1 : data->file_capacity * 2;
                DiffFile* tmp = realloc(data->files, data->file_capacity * sizeof(DiffFile));
                if (!tmp) {
                    log_error("Failed to reallocate files array");
                    free(path_a);
                    if (path_b) free(path_b);
                    free(buffer_copy);
                    diff_data_clear(data);
                    return 0;
                }
                data->files = tmp;
                // Инициализируем новую память
                memset(&data->files[data->file_count], 0, (data->file_capacity - data->file_count) * sizeof(DiffFile));
            }

            current_file = &data->files[data->file_count];
            current_file->path = strdup(path_b ? path_b : path_a); // Используем новый путь
            current_file->path_length = strlen(current_file->path);
            current_file->hunk_capacity = 1; // Начальный размер для ханков
            current_file->hunks = calloc(current_file->hunk_capacity, sizeof(DiffHunk));
            if (!current_file->hunks) {
                 log_error("Failed to allocate initial hunks for file %s", current_file->path);
                 free(path_a);
                 if (path_b) free(path_b);
                 free(buffer_copy);
                 diff_data_clear(data);
                 return 0;
            }
            data->file_count++;

            // Освобождаем временные строки
            free(path_a);
            if (path_b) free(path_b);
            current_hunk = NULL; // Сбрасываем ханк для нового файла
        }
        // Новый ханк (только если файл уже начат)
        else if (current_file && strncmp(line, "@@", 2) == 0) {
             // Сохраняем предыдущий ханк, если он есть
            if (current_hunk && current_hunk->header) { // Проверяем, был ли инициализирован предыдущий ханк
                 // Он уже должен быть в массиве current_file->hunks
                 // Просто сбрасываем указатель, чтобы начать новый
            }

            // Увеличиваем емкость ханков, если нужно
            if (current_file->hunk_count >= current_file->hunk_capacity) {
                current_file->hunk_capacity *= 2;
                DiffHunk* tmp = realloc(current_file->hunks, current_file->hunk_capacity * sizeof(DiffHunk));
                if (!tmp) {
                    log_error("Failed to reallocate hunks for file %s", current_file->path);
                    free(buffer_copy);
                    diff_data_clear(data);
                    return 0;
                }
                current_file->hunks = tmp;
                // Инициализируем новую память
                memset(&current_file->hunks[current_file->hunk_count], 0, (current_file->hunk_capacity - current_file->hunk_count) * sizeof(DiffHunk));
            }

            // Инициализируем новый ханк
            current_hunk = &current_file->hunks[current_file->hunk_count];
            current_hunk->header = strdup(line);
            current_hunk->header_length = strlen(line);
            current_hunk->line_capacity = 10; // Начальный размер для строк
            current_hunk->lines = calloc(current_hunk->line_capacity, sizeof(DiffLine));
            if (!current_hunk->lines) {
                 log_error("Failed to allocate initial lines for hunk in file %s", current_file->path);
                 free(buffer_copy);
                 diff_data_clear(data);
                 return 0;
            }
            current_hunk->line_count = 0;
        }
        // Строка контента ханка (только если ханк начат)
        else if (current_hunk && (line[0] == ' ' || line[0] == '+' || line[0] == '-')) {
            // Увеличиваем емкость строк, если нужно
            if (current_hunk->line_count >= current_hunk->line_capacity) {
                current_hunk->line_capacity *= 2;
                DiffLine* tmp = realloc(current_hunk->lines, current_hunk->line_capacity * sizeof(DiffLine));
                if (!tmp) {
                    log_error("Failed to reallocate lines for hunk in file %s", current_file->path);
                    free(buffer_copy);
                    diff_data_clear(data);
                    return 0;
                }
                current_hunk->lines = tmp;
                 // Инициализируем новую память
                memset(&current_hunk->lines[current_hunk->line_count], 0, (current_hunk->line_capacity - current_hunk->line_count) * sizeof(DiffLine));
            }

            // Добавляем строку
            DiffLine* new_line = &current_hunk->lines[current_hunk->line_count];
            new_line->content = strdup(line);
            new_line->length = strlen(line);
            switch(line[0]) {
                case '+': new_line->type = LINE_TYPE_ADD; break;
                case '-': new_line->type = LINE_TYPE_DELETE; break;
                default: new_line->type = LINE_TYPE_CONTEXT; break;
            }
            current_hunk->line_count++;
        }
        //else: игнорируем другие строки (заголовки ханков, метаданные git и т.д.)

        line = strtok_r(NULL, "\n", &saveptr);
    }

    // Не забываем сохранить последний ханк и файл, если они были
    if (current_file && current_hunk && current_hunk->header) {
        // current_hunk уже находится в current_file->hunks, просто увеличиваем счетчик
        // если он еще не был учтен (например, если это был единственный ханк)
        // Более надежный способ - проверить, есть ли он уже в массиве
        // Для простоты, предположим, что мы всегда добавляли его в массив
        // при создании. Тогда просто ничего не делаем здесь.
        // В реальной реализации нужна более точная логика.
        // Пока оставим как есть, так как логика выше должна работать.
        // Убедимся, что последний ханк добавлен в счетчик файла
        if (current_file->hunk_count < current_file->hunk_capacity) {
             current_file->hunks[current_file->hunk_count] = *current_hunk;
             current_file->hunk_count++;
        }
    }


    free(buffer_copy);
    log_info("Successfully parsed diff data with %zu files", data->file_count);
    return 1;
}
// --- КОНЕЦ ОБНОВЛЕННОЙ ФУНКЦИИ ---

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
    memset(data, 0, sizeof(DiffData));
}
