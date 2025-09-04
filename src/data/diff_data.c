#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"

#include <stdlib.h>
#include <string.h>
#include <cjson/cjson.h>

DiffData* diff_data_create(void) {
    DiffData* data = malloc(sizeof(DiffData));
    if (!data) {
        log_error("Failed to allocate memory for DiffData");
        return NULL;
    }
    
    memset(data, 0, sizeof(DiffData));
    data->file_capacity = MAX_FILES;
    data->files = malloc(data->file_capacity * sizeof(DiffFile));
    if (!data->files) {
        log_error("Failed to allocate memory for DiffFile array");
        free(data);
        return NULL;
    }
    
    memset(data->files, 0, data->file_capacity * sizeof(DiffFile));
    
    return data;
}

void diff_data_destroy(DiffData* data) {
    if (!data) {
        return;
    }
    
    diff_data_clear(data);
    free(data->files);
    free(data);
}

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
    
    memset(data->files, 0, data->file_capacity * sizeof(DiffFile));
    data->file_count = 0;
}

static LineType parse_line_type(const char* prefix) {
    if (prefix[0] == '+') {
        return LINE_TYPE_ADD;
    } else if (prefix[0] == '-') {
        return LINE_TYPE_DELETE;
    } else {
        return LINE_TYPE_CONTEXT;
    }
}

static int parse_hunk_json(DiffHunk* hunk, cJSON* hunk_json) {
    // Parse header
    cJSON* header_json = cJSON_GetObjectItem(hunk_json, "header");
    if (header_json && cJSON_IsString(header_json)) {
        hunk->header_length = strlen(header_json->valuestring);
        hunk->header = malloc(hunk->header_length + 1);
        if (!hunk->header) {
            log_error("Failed to allocate memory for hunk header");
            return 0;
        }
        strcpy(hunk->header, header_json->valuestring);
    }
    
    // Parse lines
    cJSON* lines_json = cJSON_GetObjectItem(hunk_json, "lines");
    if (lines_json && cJSON_IsArray(lines_json)) {
        hunk->line_count = cJSON_GetArraySize(lines_json);
        hunk->line_capacity = hunk->line_count;
        
        if (hunk->line_count > 0) {
            hunk->lines = malloc(hunk->line_count * sizeof(DiffLine));
            if (!hunk->lines) {
                log_error("Failed to allocate memory for DiffLine array");
                free(hunk->header);
                hunk->header = NULL;
                return 0;
            }
            
            memset(hunk->lines, 0, hunk->line_count * sizeof(DiffLine));
            
            for (size_t i = 0; i < hunk->line_count; i++) {
                cJSON* line_json = cJSON_GetArrayItem(lines_json, i);
                if (line_json && cJSON_IsString(line_json)) {
                    const char* line_content = line_json->valuestring;
                    size_t line_length = strlen(line_content);
                    
                    hunk->lines[i].type = parse_line_type(line_content);
                    hunk->lines[i].length = line_length;
                    hunk->lines[i].content = malloc(line_length + 1);
                    if (!hunk->lines[i].content) {
                        log_error("Failed to allocate memory for line content");
                        // Cleanup already allocated lines
                        for (size_t j = 0; j < i; j++) {
                            free(hunk->lines[j].content);
                        }
                        free(hunk->lines);
                        free(hunk->header);
                        hunk->header = NULL;
                        hunk->lines = NULL;
                        return 0;
                    }
                    strcpy(hunk->lines[i].content, line_content);
                }
            }
        }
    }
    
    return 1;
}

static int parse_file_json(DiffFile* file, cJSON* file_json) {
    // Parse path
    cJSON* path_json = cJSON_GetObjectItem(file_json, "path");
    if (path_json && cJSON_IsString(path_json)) {
        file->path_length = strlen(path_json->valuestring);
        file->path = malloc(file->path_length + 1);
        if (!file->path) {
            log_error("Failed to allocate memory for file path");
            return 0;
        }
        strcpy(file->path, path_json->valuestring);
    }
    
    // Parse hunks
    cJSON* hunks_json = cJSON_GetObjectItem(file_json, "hunks");
    if (hunks_json && cJSON_IsArray(hunks_json)) {
        file->hunk_count = cJSON_GetArraySize(hunks_json);
        file->hunk_capacity = file->hunk_count;
        
        if (file->hunk_count > 0) {
            file->hunks = malloc(file->hunk_count * sizeof(DiffHunk));
            if (!file->hunks) {
                log_error("Failed to allocate memory for DiffHunk array");
                free(file->path);
                file->path = NULL;
                return 0;
            }
            
            memset(file->hunks, 0, file->hunk_count * sizeof(DiffHunk));
            
            for (size_t i = 0; i < file->hunk_count; i++) {
                cJSON* hunk_json = cJSON_GetArrayItem(hunks_json, i);
                if (hunk_json && cJSON_IsObject(hunk_json)) {
                    if (!parse_hunk_json(&file->hunks[i], hunk_json)) {
                        // Cleanup already allocated hunks
                        for (size_t j = 0; j < i; j++) {
                            free(file->hunks[j].header);
                            for (size_t k = 0; k < file->hunks[j].line_count; k++) {
                                free(file->hunks[j].lines[k].content);
                            }
                            free(file->hunks[j].lines);
                        }
                        free(file->hunks);
                        free(file->path);
                        file->path = NULL;
                        file->hunks = NULL;
                        return 0;
                    }
                }
            }
        }
    }
    
    return 1;
}

int diff_data_parse_json(DiffData* data, const char* json_string) {
    if (!data || !json_string) {
        log_error("Invalid arguments to diff_data_parse_json");
        return 0;
    }
    
    // Clear existing data
    diff_data_clear(data);
    
    // Parse JSON
    cJSON* root = cJSON_Parse(json_string);
    if (!root) {
        log_error("Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return 0;
    }
    
    // Check if root is an array
    if (!cJSON_IsArray(root)) {
        log_error("JSON root is not an array");
        cJSON_Delete(root);
        return 0;
    }
    
    data->file_count = cJSON_GetArraySize(root);
    
    // Check file count limit
    if (data->file_count > MAX_FILES) {
        log_warn("File count exceeds limit, truncating to %d", MAX_FILES);
        data->file_count = MAX_FILES;
    }
    
    for (size_t i = 0; i < data->file_count; i++) {
        cJSON* file_json = cJSON_GetArrayItem(root, i);
        if (file_json && cJSON_IsObject(file_json)) {
            if (!parse_file_json(&data->files[i], file_json)) {
                log_error("Failed to parse file %zu", i);
                // Cleanup already allocated files
                for (size_t j = 0; j < i; j++) {
                    free(data->files[j].path);
                    for (size_t k = 0; k < data->files[j].hunk_count; k++) {
                        free(data->files[j].hunks[k].header);
                        for (size_t l = 0; l < data->files[j].hunks[k].line_count; l++) {
                            free(data->files[j].hunks[k].lines[l].content);
                        }
                        free(data->files[j].hunks[k].lines);
                    }
                    free(data->files[j].hunks);
                }
                data->file_count = 0;
                cJSON_Delete(root);
                return 0;
            }
        }
    }
    
    cJSON_Delete(root);
    log_info("Successfully parsed diff data with %zu files", data->file_count);
    return 1;
}
