// src/data/diff_parser.c
#include "see_code/data/diff_parser.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h> // For sscanf

// --- Вспомогательные функции для управления динамическими массивами ---
// Увеличивает емкость массива файлов при необходимости.
static int ensure_files_capacity(DiffData* data) {
    if (data->file_count < data->file_capacity) {
        return 1; // Емкости достаточно
    }
    size_t new_capacity = (data->file_capacity == 0) ? 1 : data->file_capacity * 2;
    DiffFile* tmp = realloc(data->files, new_capacity * sizeof(DiffFile));
    if (!tmp) {
        log_error("Failed to reallocate files array to capacity %zu", new_capacity);
        return 0;
    }
    // Инициализируем новую память нулями
    memset(tmp + data->file_capacity, 0, (new_capacity - data->file_capacity) * sizeof(DiffFile));
    data->files = tmp;
    data->file_capacity = new_capacity;
    log_debug("Files array capacity increased to %zu", new_capacity);
    return 1;
}

// Увеличивает емкость массива ханков для файла при необходимости.
static int ensure_hunks_capacity(DiffFile* file) {
     if (file->hunk_count < file->hunk_capacity) {
        return 1; // Емкости достаточно
    }
    size_t new_capacity = (file->hunk_capacity == 0) ? 1 : file->hunk_capacity * 2;
    DiffHunk* tmp = realloc(file->hunks, new_capacity * sizeof(DiffHunk));
    if (!tmp) {
        log_error("Failed to reallocate hunks array for file to capacity %zu", new_capacity);
        return 0;
    }
    memset(tmp + file->hunk_capacity, 0, (new_capacity - file->hunk_capacity) * sizeof(DiffHunk));
    file->hunks = tmp;
    file->hunk_capacity = new_capacity;
    log_debug("Hunks array capacity for file increased to %zu", new_capacity);
    return 1;
}

// Увеличивает емкость массива строк для ханка при необходимости.
static int ensure_lines_capacity(DiffHunk* hunk) {
     if (hunk->line_count < hunk->line_capacity) {
        return 1; // Емкости достаточно
    }
    size_t new_capacity = (hunk->line_capacity == 0) ? 10 : hunk->line_capacity * 2; // Начинаем с 10
    DiffLine* tmp = realloc(hunk->lines, new_capacity * sizeof(DiffLine));
    if (!tmp) {
        log_error("Failed to reallocate lines array for hunk to capacity %zu", new_capacity);
        return 0;
    }
    memset(tmp + hunk->line_capacity, 0, (new_capacity - hunk->line_capacity) * sizeof(DiffLine));
    hunk->lines = tmp;
    hunk->line_capacity = new_capacity;
    log_debug("Lines array capacity for hunk increased to %zu", new_capacity);
    return 1;
}
// --- Конец вспомогательных функций ---

int diff_parser_parse(DiffData* data, const char* buffer, size_t buffer_size) {
    if (!data || !buffer || buffer_size == 0) {
        log_error("Invalid arguments to diff_parser_parse");
        return 0;
    }

    // Очищаем существующие данные
    // Предполагается, что diff_data_clear вызывается в diff_data_load_from_buffer
    // diff_data_clear(data);

    // Создаем временную строку, завершенную нулем, для удобства обработки строк
    // ВАЖНО: buffer_size не включает завершающий ноль, если он не был частью исходных данных.
    // Мы копируем buffer_size байтов и добавляем ноль в конце.
    char* buffer_copy = malloc(buffer_size + 1);
    if (!buffer_copy) {
        log_error("Failed to allocate memory for buffer copy");
        return 0;
    }
    memcpy(buffer_copy, buffer, buffer_size);
    buffer_copy[buffer_size] = '\0'; // Завершаем строку

    char *saveptr = NULL;
    char *line = strtok_r(buffer_copy, "\n", &saveptr);

    DiffFile* current_file = NULL;
    DiffHunk* current_hunk = NULL;

    while (line != NULL) {
        // --- Новый файл ---
        char *path_a = NULL, *path_b = NULL;
        // Ищем строку вида "diff --git a/path b/path"
        // Используем %ms для автоматического выделения памяти под строки
        if (sscanf(line, "diff --git a/%ms b/%ms", &path_a, &path_b) == 2) {
            log_debug("Found new file: a/%s b/%s", path_a ? path_a : "NULL", path_b ? path_b : "NULL");

            // Добавляем новый файл
            if (!ensure_files_capacity(data)) {
                free(path_a);
                if (path_b) free(path_b);
                free(buffer_copy);
                // diff_data_clear(data); // Предполагается, что вызывающая функция обработает это
                return 0;
            }

            current_file = &data->files[data->file_count]; // Указатель на новый слот
            // Используем path_b как основное имя файла, если оно есть и не пустое, иначе path_a
            char* chosen_path = NULL;
            if (path_b && strlen(path_b) > 0) {
                chosen_path = path_b;
                free(path_a); // Освобождаем ненужный путь
                path_a = NULL;
            } else if (path_a && strlen(path_a) > 0) {
                chosen_path = path_a;
                free(path_b); // Освобождаем ненужный путь
                path_b = NULL;
            } else {
                // Оба пути пусты или NULL, используем пустую строку
                chosen_path = strdup(""); // Создаем пустую строку
                free(path_a);
                free(path_b);
                path_a = NULL;
                path_b = NULL;
            }

            current_file->path = chosen_path; // Передаем владение памятью
            current_file->path_length = current_file->path ? strlen(current_file->path) : 0;
            // Инициализируем массив ханков для нового файла
            current_file->hunk_capacity = 1;
            current_file->hunks = calloc(current_file->hunk_capacity, sizeof(DiffHunk));
            if (!current_file->hunks && current_file->hunk_capacity > 0) {
                 log_error("Failed to allocate initial hunks for file %s", current_file->path ? current_file->path : "unknown");
                 // Освобождаем path, так как файл не был добавлен в data->file_count
                 free(current_file->path);
                 current_file->path = NULL;
                 free(buffer_copy);
                 // diff_data_clear(data);
                 return 0;
            }
            data->file_count++; // Увеличиваем счетчик файлов только после успешного добавления
            current_hunk = NULL; // Сброс ханка для нового файла
            log_debug("Added file %s, total files now: %zu", current_file->path, data->file_count);

        // --- Новый ханк ---
        // Ищем строку вида "@@ -old_start,old_count +new_start,new_count @@ context"
        } else if (current_file && strncmp(line, "@@", 2) == 0) {
            log_debug("Found new hunk: %.50s", line); // Логируем первые 50 символов
             // Добавляем новый ханк в текущий файл
            if (!ensure_hunks_capacity(current_file)) {
                free(buffer_copy);
                // diff_data_clear(data);
                return 0;
            }

            current_hunk = &current_file->hunks[current_file->hunk_count]; // Указатель на новый слот
            current_hunk->header = strdup(line);
            current_hunk->header_length = current_hunk->header ? strlen(current_hunk->header) : 0;
            // Инициализируем массив строк для нового ханка
            current_hunk->line_capacity = 10; // Начальный размер
            current_hunk->lines = calloc(current_hunk->line_capacity, sizeof(DiffLine));
            if (!current_hunk->lines && current_hunk->line_capacity > 0) {
                 log_error("Failed to allocate initial lines for hunk in file %s", current_file->path ? current_file->path : "unknown");
                 // Освобождаем header, так как ханк не был добавлен в current_file->hunk_count
                 free(current_hunk->header);
                 current_hunk->header = NULL;
                 free(buffer_copy);
                 // diff_data_clear(data);
                 return 0;
            }
            current_file->hunk_count++; // Увеличиваем счетчик ханков только после успешного добавления
            current_hunk->is_collapsed = 0; // Инициализируем состояние сворачивания
            log_debug("Added hunk to file %s, total hunks now: %zu", current_file->path, current_file->hunk_count);

        // --- Строка контента ханка ---
        // Строки, начинающиеся на ' ', '+', '-'
        } else if (current_hunk && (line[0] == ' ' || line[0] == '+' || line[0] == '-')) {
            // log_debug("Found line: %.30s", line); // Логируем первые 30 символов каждой строки - может быть много
            // Добавляем строку в текущий ханк
            if (!ensure_lines_capacity(current_hunk)) {
                free(buffer_copy);
                // diff_data_clear(data);
                return 0;
            }

            DiffLine* new_line = &current_hunk->lines[current_hunk->line_count]; // Указатель на новый слот
            new_line->content = strdup(line);
            new_line->length = new_line->content ? strlen(new_line->content) : 0;
            switch(line[0]) {
                case '+': new_line->type = LINE_TYPE_ADD; break;
                case '-': new_line->type = LINE_TYPE_DELETE; break;
                default: new_line->type = LINE_TYPE_CONTEXT; break;
            }
            current_hunk->line_count++; // Увеличиваем счетчик строк только после успешного добавления
            // log_debug("Added line to hunk, total lines now: %zu", current_hunk->line_count);

        }
        // --- Иначе игнорируем строку ---
        // Это могут быть заголовки других частей diff (index, ---, +++), контекстные строки,
        // бинарные файлы и т.д. Пока их не обрабатываем.

        line = strtok_r(NULL, "\n", &saveptr);
    }

    free(buffer_copy);
    log_info("Successfully parsed diff data with %zu files", data->file_count);
    return 1;
}
