#include <termux-gui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>
#include <cjson/cJSON.h>

#define SOCKET_PATH "/data/data/com.termux/files/usr/tmp/see_code_socket"
#define INITIAL_BUFFER_SIZE 16384
#define MAX_MESSAGE_SIZE 10485760  // 10MB max

typedef struct {
    int id;
    char* header;
    char** lines;
    int num_lines;
    int is_collapsed;
    float render_y;
    float render_h;
} Hunk;

typedef struct {
    char* path;
    Hunk* hunks;
    int num_hunks;
    int hunks_capacity;
} DiffFile;

typedef struct {
    DiffFile* files;
    int num_files;
    int files_capacity;
    float scroll_y;
    float total_height;
    int needs_redraw;
} UIState;

static UIState g_state = {0};
static tgui_connection* g_conn = NULL;
static tgui_activity* g_activity = NULL;
static int g_socket_fd = -1;
static pthread_t g_socket_thread;
static pthread_mutex_t g_state_mutex = PTHREAD_MUTEX_INITIALIZER;

void free_hunk(Hunk* hunk) {
    if (hunk->header) {
        free(hunk->header);
        hunk->header = NULL;
    }
    
    if (hunk->lines) {
        for (int i = 0; i < hunk->num_lines; i++) {
            if (hunk->lines[i]) {
                free(hunk->lines[i]);
            }
        }
        free(hunk->lines);
        hunk->lines = NULL;
    }
    hunk->num_lines = 0;
}

void free_diff_file(DiffFile* file) {
    if (file->path) {
        free(file->path);
        file->path = NULL;
    }
    
    if (file->hunks) {
        for (int i = 0; i < file->num_hunks; i++) {
            free_hunk(&file->hunks[i]);
        }
        free(file->hunks);
        file->hunks = NULL;
    }
    file->num_hunks = 0;
    file->hunks_capacity = 0;
}

void free_ui_state(UIState* state) {
    if (state->files) {
        for (int i = 0; i < state->num_files; i++) {
            free_diff_file(&state->files[i]);
        }
        free(state->files);
        state->files = NULL;
    }
    state->num_files = 0;
    state->files_capacity = 0;
}

char* safe_strdup(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dst = malloc(len + 1);
    if (dst) {
        strcpy(dst, src);
    }
    return dst;
}

int ensure_files_capacity(UIState* state, int needed) {
    if (state->files_capacity >= needed) return 1;
    
    int new_capacity = needed + 16;
    DiffFile* new_files = realloc(state->files, new_capacity * sizeof(DiffFile));
    if (!new_files) {
        printf("Failed to allocate memory for files\n");
        return 0;
    }
    
    memset(new_files + state->files_capacity, 0, 
           (new_capacity - state->files_capacity) * sizeof(DiffFile));
    
    state->files = new_files;
    state->files_capacity = new_capacity;
    return 1;
}

int ensure_hunks_capacity(DiffFile* file, int needed) {
    if (file->hunks_capacity >= needed) return 1;
    
    int new_capacity = needed + 16;
    Hunk* new_hunks = realloc(file->hunks, new_capacity * sizeof(Hunk));
    if (!new_hunks) {
        printf("Failed to allocate memory for hunks\n");
        return 0;
    }
    
    memset(new_hunks + file->hunks_capacity, 0,
           (new_capacity - file->hunks_capacity) * sizeof(Hunk));
    
    file->hunks = new_hunks;
    file->hunks_capacity = new_capacity;
    return 1;
}

void parse_json_diff(const char* json_str) {
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        printf("Failed to parse JSON: %s\n", cJSON_GetErrorPtr());
        return;
    }
    
    pthread_mutex_lock(&g_state_mutex);
    
    // Free existing state
    free_ui_state(&g_state);
    
    cJSON *files_array = cJSON_GetObjectItem(json, "files");
    if (!files_array || !cJSON_IsArray(files_array)) {
        printf("Invalid or missing 'files' array in JSON\n");
        cJSON_Delete(json);
        pthread_mutex_unlock(&g_state_mutex);
        return;
    }
    
    int num_files = cJSON_GetArraySize(files_array);
    if (!ensure_files_capacity(&g_state, num_files)) {
        cJSON_Delete(json);
        pthread_mutex_unlock(&g_state_mutex);
        return;
    }
    
    g_state.num_files = 0;
    
    cJSON *file_json = NULL;
    cJSON_ArrayForEach(file_json, files_array) {
        DiffFile* file = &g_state.files[g_state.num_files];
        
        cJSON *path_json = cJSON_GetObjectItem(file_json, "path");
        if (path_json && cJSON_IsString(path_json)) {
            file->path = safe_strdup(path_json->valuestring);
        } else {
            file->path = safe_strdup("unknown_file");
        }
        
        cJSON *hunks_array = cJSON_GetObjectItem(file_json, "hunks");
        if (hunks_array && cJSON_IsArray(hunks_array)) {
            int num_hunks = cJSON_GetArraySize(hunks_array);
            if (ensure_hunks_capacity(file, num_hunks)) {
                file->num_hunks = 0;
                
                cJSON *hunk_json = NULL;
                cJSON_ArrayForEach(hunk_json, hunks_array) {
                    Hunk* hunk = &file->hunks[file->num_hunks];
                    
                    cJSON *id_json = cJSON_GetObjectItem(hunk_json, "id");
                    if (id_json && cJSON_IsNumber(id_json)) {
                        hunk->id = id_json->valueint;
                    }
                    
                    cJSON *header_json = cJSON_GetObjectItem(hunk_json, "header");
                    if (header_json && cJSON_IsString(header_json)) {
                        hunk->header = safe_strdup(header_json->valuestring);
                    } else {
                        hunk->header = safe_strdup("@@ unknown @@");
                    }
                    
                    cJSON *collapsed_json = cJSON_GetObjectItem(hunk_json, "is_collapsed");
                    if (collapsed_json && cJSON_IsBool(collapsed_json)) {
                        hunk->is_collapsed = cJSON_IsTrue(collapsed_json);
                    } else {
                        hunk->is_collapsed = 1;
                    }
                    
                    cJSON *lines_array = cJSON_GetObjectItem(hunk_json, "lines");
                    if (lines_array && cJSON_IsArray(lines_array)) {
                        int num_lines = cJSON_GetArraySize(lines_array);
                        hunk->lines = malloc(num_lines * sizeof(char*));
                        if (hunk->lines) {
                            hunk->num_lines = 0;
                            
                            cJSON *line_json = NULL;
                            cJSON_ArrayForEach(line_json, lines_array) {
                                if (cJSON_IsString(line_json)) {
                                    hunk->lines[hunk->num_lines] = safe_strdup(line_json->valuestring);
                                    if (hunk->lines[hunk->num_lines]) {
                                        hunk->num_lines++;
                                    }
                                }
                            }
                        }
                    }
                    
                    file->num_hunks++;
                }
            }
        }
        
        g_state.num_files++;
    }
    
    g_state.needs_redraw = 1;
    
    cJSON_Delete(json);
    pthread_mutex_unlock(&g_state_mutex);
    
    printf("Parsed %d files with diff data\n", g_state.num_files);
}

char* receive_full_message(int client_fd) {
    uint32_t msg_len;
    ssize_t bytes = recv(client_fd, &msg_len, sizeof(msg_len), MSG_WAITALL);
    if (bytes != sizeof(msg_len)) {
        return NULL;
    }
    
    if (msg_len > MAX_MESSAGE_SIZE) {
        printf("Message too large: %u bytes\n", msg_len);
        return NULL;
    }
    
    char* buffer = malloc(msg_len + 1);
    if (!buffer) {
        printf("Failed to allocate %u bytes for message\n", msg_len);
        return NULL;
    }
    
    bytes = recv(client_fd, buffer, msg_len, MSG_WAITALL);
    if (bytes != (ssize_t)msg_len) {
        free(buffer);
        return NULL;
    }
    
    buffer[msg_len] = '\0';
    return buffer;
}

void* socket_thread(void* arg) {
    struct sockaddr_un addr;
    
    g_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socket_fd == -1) {
        printf("Socket creation failed: %s\n", strerror(errno));
        return NULL;
    }
    
    unlink(SOCKET_PATH);
    
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);
    
    if (bind(g_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        printf("Bind failed: %s\n", strerror(errno));
        close(g_socket_fd);
        return NULL;
    }
    
    if (listen(g_socket_fd, 1) == -1) {
        printf("Listen failed: %s\n", strerror(errno));
        close(g_socket_fd);
        return NULL;
    }
    
    printf("Socket listening on %s\n", SOCKET_PATH);
    
    while (1) {
        int client_fd = accept(g_socket_fd, NULL, NULL);
        if (client_fd == -1) continue;
        
        char* message = receive_full_message(client_fd);
        if (message) {
            printf("Received message of %zu bytes\n", strlen(message));
            parse_json_diff(message);
            free(message);
        }
        
        close(client_fd);
    }
    
    return NULL;
}

void draw_ui() {
    if (!g_activity) return;
    
    pthread_mutex_lock(&g_state_mutex);
    
    if (!g_state.needs_redraw) {
        pthread_mutex_unlock(&g_state_mutex);
        return;
    }
    
    tgui_clear_views(g_activity);
    
    float y_pos = 50.0f - g_state.scroll_y;
    
    for (int i = 0; i < g_state.num_files; i++) {
        DiffFile* file = &g_state.files[i];
        
        if (file->path) {
            tgui_view* file_header = tgui_create_textview(g_activity, file->path);
            tgui_set_view_position(file_header, 10, (int)y_pos, 1000, 40);
            tgui_set_textview_textsize(file_header, 16);
        }
        y_pos += 50.0f;
        
        for (int j = 0; j < file->num_hunks; j++) {
            Hunk* hunk = &file->hunks[j];
            hunk->render_y = y_pos;
            
            if (hunk->header) {
                tgui_view* hunk_btn = tgui_create_button(g_activity, hunk->header);
                tgui_set_view_position(hunk_btn, 20, (int)y_pos, 1000, 35);
                tgui_set_view_id(hunk_btn, i * 10000 + j);
            }
            y_pos += 40.0f;
            
            hunk->render_h = 40.0f;
            
            if (!hunk->is_collapsed && hunk->lines) {
                for (int k = 0; k < hunk->num_lines; k++) {
                    if (hunk->lines[k]) {
                        tgui_view* line_view = tgui_create_textview(g_activity, hunk->lines[k]);
                        tgui_set_view_position(line_view, 40, (int)y_pos, 1000, 30);
                        tgui_set_textview_textsize(line_view, 12);
                        
                        if (hunk->lines[k][0] == '+') {
                            tgui_set_textview_textcolor(line_view, 0xFF00FF00);
                        } else if (hunk->lines[k][0] == '-') {
                            tgui_set_textview_textcolor(line_view, 0xFFFF0000);
                        }
                        
                        y_pos += 35.0f;
                    }
                }
                hunk->render_h += hunk->num_lines * 35.0f;
            }
        }
    }
    
    g_state.total_height = y_pos + g_state.scroll_y;
    g_state.needs_redraw = 0;
    
    pthread_mutex_unlock(&g_state_mutex);
}

void handle_button_click(int view_id) {
    pthread_mutex_lock(&g_state_mutex);
    
    int file_idx = view_id / 10000;
    int hunk_idx = view_id % 10000;
    
    if (file_idx < g_state.num_files && hunk_idx < g_state.files[file_idx].num_hunks) {
        Hunk* hunk = &g_state.files[file_idx].hunks[hunk_idx];
        hunk->is_collapsed = !hunk->is_collapsed;
        g_state.needs_redraw = 1;
    }
    
    pthread_mutex_unlock(&g_state_mutex);
}

int main() {
    printf("Starting see_code GUI application...\n");
    
    g_conn = tgui_create();
    if (!g_conn) {
        printf("ERROR: Failed to create termux-gui connection\n");
        printf("SOLUTION: Make sure Termux:GUI app is installed and running\n");
        return 1;
    }
    
    g_activity = tgui_create_activity(g_conn, 0);
    if (!g_activity) {
        printf("ERROR: Failed to create activity\n");
        printf("SOLUTION: Check Termux:GUI permissions\n");
        tgui_destroy(g_conn);
        return 1;
    }
    
    tgui_set_activity_orientation(g_activity, 1);
    
    if (pthread_create(&g_socket_thread, NULL, socket_thread, NULL) != 0) {
        printf("ERROR: Failed to create socket thread\n");
        printf("SOLUTION: Check system resources and permissions\n");
        tgui_destroy(g_conn);
        return 1;
    }
    
    printf("SUCCESS: GUI initialized. Send data from Neovim using :SeeCodeDiff\n");
    
    while (1) {
        tgui_event* event = tgui_wait_event(g_conn);
        if (!event) continue;
        
        if (event->type == TGUI_EVENT_CLICK) {
            handle_button_click(event->view_id);
            draw_ui();
        } else if (event->type == TGUI_EVENT_SCROLL) {
            pthread_mutex_lock(&g_state_mutex);
            g_state.scroll_y += event->scroll_dy * 10.0f;
            if (g_state.scroll_y < 0) g_state.scroll_y = 0;
            g_state.needs_redraw = 1;
            pthread_mutex_unlock(&g_state_mutex);
            draw_ui();
        }
        
        tgui_free_event(event);
        
        // Redraw if needed
        draw_ui();
    }
    
    // Cleanup
    free_ui_state(&g_state);
    close(g_socket_fd);
    pthread_join(g_socket_thread, NULL);
    pthread_mutex_destroy(&g_state_mutex);
    tgui_destroy(g_conn);
    
    return 0;
}
