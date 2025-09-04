#include "see_code/core/app.h"
#include "see_code/gui/renderer.h"
#include "see_code/gui/ui_manager.h"
#include "see_code/network/socket_server.h"
#include "see_code/data/diff_data.h"
#include "see_code/utils/logger.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AppConfig config;
    int running;
    int initialized;
    
    // Component handles
    SocketServer* socket_server;
    Renderer* renderer;
    UIManager* ui_manager;
    DiffData* diff_data;
    
    // Threading
    pthread_mutex_t state_mutex;
    pthread_t socket_thread;
    
    // Application state
    float scroll_y;
    int needs_redraw;
    
} AppState;

static AppState g_app = {0};

// Socket callback - called when data is received
static void on_socket_data(const char* json_data, size_t length) {
    log_info("Received %zu bytes from Neovim", length);
    
    pthread_mutex_lock(&g_app.state_mutex);
    
    if (diff_data_parse_json(g_app.diff_data, json_data)) {
        log_info("Successfully parsed diff data");
        g_app.needs_redraw = 1;
        
        // Update UI with new data
        ui_manager_set_diff_data(g_app.ui_manager, g_app.diff_data);
    } else {
        log_error("Failed to parse JSON data from Neovim");
    }
    
    pthread_mutex_unlock(&g_app.state_mutex);
}

int app_init(const AppConfig* config) {
    if (!config) {
        log_error("Null configuration provided");
        return 0;
    }
    
    if (g_app.initialized) {
        log_warn("Application already initialized");
        return 1;
    }
    
    // Copy configuration
    memcpy(&g_app.config, config, sizeof(AppConfig));
    
    // Initialize threading primitives
    if (pthread_mutex_init(&g_app.state_mutex, NULL) != 0) {
        log_error("Failed to initialize state mutex");
        return 0;
    }
    
    log_info("Initializing application components...");
    
    // Initialize diff data storage
    g_app.diff_data = diff_data_create();
    if (!g_app.diff_data) {
        log_error("Failed to initialize diff data storage");
        goto cleanup;
    }
    
    // Initialize renderer
    g_app.renderer = renderer_create(config->window_width, config->window_height);
    if (!g_app.renderer) {
        log_error("Failed to initialize renderer");
        goto cleanup;
    }
    
    // Initialize UI manager
    g_app.ui_manager = ui_manager_create(g_app.renderer);
    if (!g_app.ui_manager) {
        log_error("Failed to initialize UI manager");
        goto cleanup;
    }
    
    // Initialize socket server
    g_app.socket_server = socket_server_create(config->socket_path, on_socket_data);
    if (!g_app.socket_server) {
        log_error("Failed to initialize socket server");
        goto cleanup;
    }
    
    // Start socket server thread
    if (pthread_create(&g_app.socket_thread, NULL, 
                      (void*(*)(void*))socket_server_run, g_app.socket_server) != 0) {
        log_error("Failed to create socket thread");
        goto cleanup;
    }
    
    g_app.running = 1;
    g_app.initialized = 1;
    g_app.needs_redraw = 1;
    
    log_info("Application initialization completed");
    return 1;
    
cleanup:
    app_cleanup();
    return 0;
}

int app_update(void) {
    if (!g_app.initialized || !g_app.running) {
        return 0;
    }
    
    pthread_mutex_lock(&g_app.state_mutex);
    
    // Handle renderer updates
    if (!renderer_begin_frame(g_app.renderer)) {
        log_error("Failed to begin renderer frame");
        pthread_mutex_unlock(&g_app.state_mutex);
        return 0;
    }
    
    // Update UI if needed
    if (g_app.needs_redraw) {
        ui_manager_update_layout(g_app.ui_manager, g_app.scroll_y);
        g_app.needs_redraw = 0;
    }
    
    // Render UI
    ui_manager_render(g_app.ui_manager);
    
    // End frame
    if (!renderer_end_frame(g_app.renderer)) {
        log_error("Failed to end renderer frame");
        pthread_mutex_unlock(&g_app.state_mutex);
        return 0;
    }
    
    pthread_mutex_unlock(&g_app.state_mutex);
    
    return 1;
}

void app_shutdown(void) {
    log_info("Initiating application shutdown");
    g_app.running = 0;
    
    // Stop socket server
    if (g_app.socket_server) {
        socket_server_stop(g_app.socket_server);
    }
}

void app_cleanup(void) {
    if (!g_app.initialized) {
        return;
    }
    
    log_info("Cleaning up application resources");
    
    // Wait for socket thread to finish
    if (g_app.socket_thread) {
        pthread_join(g_app.socket_thread, NULL);
    }
    
    // Clean up components
    if (g_app.socket_server) {
        socket_server_destroy(g_app.socket_server);
        g_app.socket_server = NULL;
    }
    
    if (g_app.ui_manager) {
        ui_manager_destroy(g_app.ui_manager);
        g_app.ui_manager = NULL;
    }
    
    if (g_app.renderer) {
        renderer_destroy(g_app.renderer);
        g_app.renderer = NULL;
    }
    
    if (g_app.diff_data) {
        diff_data_destroy(g_app.diff_data);
        g_app.diff_data = NULL;
    }
    
    // Clean up threading
    pthread_mutex_destroy(&g_app.state_mutex);
    
    g_app.initialized = 0;
    g_app.running = 0;
    
    log_info("Application cleanup completed");
}

int app_is_running(void) {
    return g_app.running;
}

const AppConfig* app_get_config(void) {
    return g_app.initialized ? &g_app.config : NULL;
}

void app_handle_touch(float x, float y) {
    if (!g_app.initialized) return;
    
    pthread_mutex_lock(&g_app.state_mutex);
    
    log_debug("Touch event at (%.2f, %.2f)", x, y);
    
    // Convert touch coordinates and handle UI interaction
    if (ui_manager_handle_touch(g_app.ui_manager, x, y + g_app.scroll_y)) {
        g_app.needs_redraw = 1;
        log_debug("Touch handled by UI manager");
    }
    
    pthread_mutex_unlock(&g_app.state_mutex);
}

void app_handle_scroll(float dx, float dy) {
    if (!g_app.initialized) return;
    
    pthread_mutex_lock(&g_app.state_mutex);
    
    g_app.scroll_y += dy * SCROLL_SENSITIVITY;
    
    // Clamp scroll position
    if (g_app.scroll_y < 0) {
        g_app.scroll_y = 0;
    }
    
    float max_scroll = ui_manager_get_content_height(g_app.ui_manager) - g_app.config.window_height;
    if (max_scroll > 0 && g_app.scroll_y > max_scroll) {
        g_app.scroll_y = max_scroll;
    }
    
    g_app.needs_redraw = 1;
    log_debug("Scroll updated: y=%.2f", g_app.scroll_y);
    
    pthread_mutex_unlock(&g_app.state_mutex);
}

void app_handle_resize(int width, int height) {
    if (!g_app.initialized) return;
    
    pthread_mutex_lock(&g_app.state_mutex);
    
    log_info("Window resized to %dx%d", width, height);
    
    g_app.config.window_width = width;
    g_app.config.window_height = height;
    
    // Update renderer
    if (g_app.renderer) {
        renderer_resize(g_app.renderer, width, height);
    }
    
    g_app.needs_redraw = 1;
    
    pthread_mutex_unlock(&g_app.state_mutex);
}
