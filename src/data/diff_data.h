// src/data/diff_data.h
#ifndef SEE_CODE_DIFF_DATA_H
#define SEE_CODE_DIFF_DATA_H

#include <stddef.h> // for size_t

// Enums for line types
typedef enum {
    LINE_TYPE_CONTEXT = 0,
    LINE_TYPE_ADD,
    LINE_TYPE_DELETE
} DiffLineType;

// Structure to hold information about a single line in a diff hunk
typedef struct {
    char* content;
    size_t length;
    DiffLineType type;
} DiffLine;

// Structure to hold information about a diff hunk
typedef struct {
    char* header;
    size_t header_length;
    DiffLine* lines;
    size_t line_count;
    size_t line_capacity; // For potential dynamic resizing
    // --- Добавлено для сворачивания ---
    int is_collapsed; // 0 = развернут, 1 = свернут
    // --- Конец добавления ---
} DiffHunk;

// Structure to hold information about a file in the diff
typedef struct {
    char* path;
    size_t path_length;
    DiffHunk* hunks;
    size_t hunk_count;
    size_t hunk_capacity; // For potential dynamic resizing
    // --- Добавлено для сворачивания ---
    int is_collapsed; // 0 = развернут, 1 = свернут
    // --- Конец добавления ---
} DiffFile;

// Structure to hold the entire diff data
typedef struct {
    DiffFile* files;
    size_t file_count;
    size_t file_capacity; // For potential dynamic resizing
} DiffData;

// Function declarations
DiffData* diff_data_create(void);
void diff_data_destroy(DiffData* data);
int diff_data_load_from_buffer(DiffData* data, const char* buffer, size_t buffer_size);
void diff_data_clear(DiffData* data);

#endif // SEE_CODE_DIFF_DATA_H
