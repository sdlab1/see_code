// src/data/diff_parser.c
// УЛУЧШЕНИЕ: Парсер теперь корректно обрабатывает имена файлов с пробелами (в кавычках).
#include "see_code/data/diff_parser.h"
#include "see_code/utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
static int ensure_files_capacity(DiffData* data) {
    if (data->file_count >= data->file_capacity) {
        size_t new_cap = data->file_capacity == 0 ? 8 : data->file_capacity * 2;
        DiffFile* new_files = realloc(data->files, new_cap * sizeof(DiffFile));
        if (!new_files) return 0;
        data->files = new_files;
        data->file_capacity = new_cap;
    }
    return 1;
}
static int ensure_hunks_capacity(DiffFile* file) {
    if (file->hunk_count >= file->hunk_capacity) {
        size_t new_cap = file->hunk_capacity == 0 ? 8 : file->hunk_capacity * 2;
        DiffHunk* new_hunks = realloc(file->hunks, new_cap * sizeof(DiffHunk));
        if (!new_hunks) return 0;
        file->hunks = new_hunks;
        file->hunk_capacity = new_cap;
    }
    return 1;
}
static int ensure_lines_capacity(DiffHunk* hunk) {
     if (hunk->line_count >= hunk->line_capacity) {
        size_t new_cap = hunk->line_capacity == 0 ? 16 : hunk->line_capacity * 2;
        DiffLine* new_lines = realloc(hunk->lines, new_cap * sizeof(DiffLine));
        if (!new_lines) return 0;
        hunk->lines = new_lines;
        hunk->line_capacity = new_cap;
    }
    return 1;
}
// Вспомогательная функция для парсинга пути из "diff --git"
// Обрабатывает как обычные пути, так и пути в кавычках
static char* parse_git_path(const char** p_ptr) {
    const char* p = *p_ptr;
    while (*p && isspace((unsigned char)*p)) p++;
    char* path_buffer = malloc(1024); // Предполагаем, что путь не длиннее
    if (!path_buffer) return NULL;
    char* q = path_buffer;
    if (*p == '"') {
        p++; // Пропускаем открывающую кавычку
        while (*p && *p != '"') {
            if (*p == '\\') {
                p++; // Пропускаем escape-символ
                if (!*p) break; // Внезапный конец строки
            }
            *q++ = *p++;
        }
        if (*p == '"') p++;
    } else {
        while (*p && !isspace((unsigned char)*p)) {
            *q++ = *p++;
        }
    }
    *q = '\0';
    *p_ptr = p;
    return path_buffer;
}
int diff_parser_parse(DiffData* data, const char* buffer, size_t buffer_size) {
    if (!data || !buffer || buffer_size == 0) return 0;
    char* buffer_copy = malloc(buffer_size + 1);
    if (!buffer_copy) return 0;
    memcpy(buffer_copy, buffer, buffer_size);
    buffer_copy[buffer_size] = '\0';
    char *saveptr = NULL;
    char *line = strtok_r(buffer_copy, "
", &saveptr);
    DiffFile* current_file = NULL;
    DiffHunk* current_hunk = NULL;
    while (line != NULL) {
        if (strncmp(line, "diff --git ", 11) == 0) {
            const char* p = line + 11;
            char* path_a_raw = parse_git_path(&p);
            char* path_b_raw = parse_git_path(&p);
            char* path_a = (strncmp(path_a_raw, "a/", 2) == 0) ? path_a_raw + 2 : path_a_raw;
            char* path_b = (strncmp(path_b_raw, "b/", 2) == 0) ? path_b_raw + 2 : path_b_raw;
            if (!ensure_files_capacity(data)) { free(path_a_raw); free(path_b_raw); return 0; }
            current_file = &data->files[data->file_count];
            memset(current_file, 0, sizeof(DiffFile));
            current_file->path = strdup(path_b);
            free(path_a_raw);
            free(path_b_raw);
            data->file_count++;
            current_hunk = NULL;
        } else if (current_file && strncmp(line, "@@", 2) == 0) {
            if (!ensure_hunks_capacity(current_file)) { return 0; }
            current_hunk = &current_file->hunks[current_file->hunk_count];
            memset(current_hunk, 0, sizeof(DiffHunk));
            current_hunk->header = strdup(line);
            current_file->hunk_count++;
        } else if (current_hunk && (line[0] == ' ' || line[0] == '+' || line[0] == '-')) {
            if (!ensure_lines_capacity(current_hunk)) { return 0; }
            DiffLine* new_line = &current_hunk->lines[current_hunk->line_count];
            new_line->content = strdup(line);
            switch(line[0]) {
                case '+': new_line->type = LINE_TYPE_ADD; break;
                case '-': new_line->type = LINE_TYPE_DELETE; break;
                default: new_line->type = LINE_TYPE_CONTEXT; break;
            }
            current_hunk->line_count++;
        }
        line = strtok_r(NULL, "
", &saveptr);
    }
    // --- ИСПРАВЛЕНИЕ: Освобождаем buffer_copy при успешном завершении ---
    free(buffer_copy);
    // --- КОНЕЦ ИСПРАВЛЕНИЯ ---
    return 1;
}
