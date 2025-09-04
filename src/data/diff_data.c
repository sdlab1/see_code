// src/data/diff_data.c
#include "see_code/data/diff_data.h"
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

// --- CHANGED FUNCTION IMPLEMENTATION ---
// This is a placeholder. You need to define the binary format.
// For now, let's assume the buffer is just the raw diff text.
// A more complex format could be: [num_files:4][file1_path_len:4][file1_path][file1_data_len:4][file1_data]...
int diff_data_load_from_buffer(DiffData* data, const char* buffer, size_t buffer_size) {
    if (!data || !buffer) {
        log_error("Invalid arguments to diff_data_load_from_buffer");
        return 0;
    }

    // Clear existing data
    diff_data_clear(data);

    // --- PLACEHOLDER IMPLEMENTATION ---
    // For demonstration, treat the entire buffer as a single "raw diff text"
    // and store it in one dummy file structure.
    // You will need to replace this with actual parsing logic based on your chosen binary format.

    data->file_count = 1;
    data->files = malloc(sizeof(DiffFile));
    if (!data->files) {
        log_error("Failed to allocate memory for DiffFile array");
        return 0;
    }
    memset(data->files, 0, sizeof(DiffFile));

    DiffFile* file = &data->files[0];
    file->path_length = 13; // strlen("raw_diff_data")
    file->path = malloc(file->path_length + 1);
    if (file->path) {
        strcpy(file->path, "raw_diff_data");
    }

    // Allocate memory for the raw buffer content
    file->hunk_count = 1; // Treat whole buffer as one hunk for now
    file->hunks = malloc(sizeof(DiffHunk));
    if (!file->hunks) {
        log_error("Failed to allocate memory for DiffHunk array");
        // Cleanup path
        free(file->path);
        file->path = NULL;
        data->file_count = 0;
        free(data->files);
        data->files = NULL;
        return 0;
    }
    memset(file->hunks, 0, sizeof(DiffHunk));

    DiffHunk* hunk = &file->hunks[0];
    hunk->header_length = 15; // strlen("[Raw Diff Data]")
    hunk->header = malloc(hunk->header_length + 1);
    if (hunk->header) {
         strcpy(hunk->header, "[Raw Diff Data]");
    }

    // Allocate one line to hold the entire buffer
    hunk->line_count = 1;
    hunk->lines = malloc(sizeof(DiffLine));
    if (!hunk->lines) {
        log_error("Failed to allocate memory for DiffLine array");
        // Cleanup hunk header and file
        free(hunk->header);
        hunk->header = NULL;
        free(file->hunks);
        file->hunks = NULL;
        file->hunk_count = 0;
        free(file->path);
        file->path = NULL;
        data->file_count = 0;
        free(data->files);
        data->files = NULL;
        return 0;
    }
    memset(hunk->lines, 0, sizeof(DiffLine));

    DiffLine* line = &hunk->lines[0];
    line->length = buffer_size;
    line->content = malloc(buffer_size + 1); // +1 for null terminator if needed as string
    if (line->content) {
        memcpy(line->content, buffer, buffer_size);
        line->content[buffer_size] = '\0'; // Null terminate for potential string use
        line->type = LINE_TYPE_CONTEXT; // Default type
    } else {
        log_error("Failed to allocate memory for raw buffer content");
        // Cleanup line, hunk, file
        free(hunk->lines);
        hunk->lines = NULL;
        hunk->line_count = 0;
        free(hunk->header);
        hunk->header = NULL;
        free(file->hunks);
        file->hunks = NULL;
        file->hunk_count = 0;
        free(file->path);
        file->path = NULL;
        data->file_count = 0;
        free(data->files);
        data->files = NULL;
        return 0;
    }

    log_info("Loaded raw buffer of %zu bytes into DiffData structure (placeholder)", buffer_size);
    return 1;
    // --- END PLACEHOLDER ---
}
// --- END CHANGE ---

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
