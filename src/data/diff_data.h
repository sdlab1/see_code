#ifndef SEE_CODE_DIFF_DATA_H
#define SEE_CODE_DIFF_DATA_H

#include <stddef.h>

// Line types
typedef enum {
    LINE_TYPE_CONTEXT = 0,
    LINE_TYPE_ADD = 1,
    LINE_TYPE_DELETE = 2
} LineType;

// Line structure
typedef struct {
    LineType type;
    char* content;
    size_t length;
} DiffLine;

// Hunk structure
typedef struct {
    char* header;
    size_t header_length;
    DiffLine* lines;
    size_t line_count;
    size_t line_capacity;
} DiffHunk;

// File structure
typedef struct {
    char* path;
    size_t path_length;
    DiffHunk* hunks;
    size_t hunk_count;
    size_t hunk_capacity;
} DiffFile;

// Diff data structure
typedef struct {
    DiffFile* files;
    size_t file_count;
    size_t file_capacity;
} DiffData;

// Diff data functions
DiffData* diff_data_create(void);
void diff_data_destroy(DiffData* data);
int diff_data_parse_json(DiffData* data, const char* json_string);
void diff_data_clear(DiffData* data);

#endif // SEE_CODE_DIFF_DATA_H
