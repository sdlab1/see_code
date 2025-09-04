#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "see_code/core/app.h"
#include "see_code/core/config.h"
#include "see_code/utils/logger.h"
#include "see_code/utils/deps_check.h"

static volatile int g_running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        log_info("Received shutdown signal");
        g_running = 0;
        app_shutdown();
    }
}

void print_usage(const char* program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("\nOptions:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --verbose  Enable verbose logging\n");
    printf("  -d, --debug    Enable debug mode\n");
    printf("  --check-deps   Check system dependencies and exit\n");
    printf("\nSee_code - Interactive Git Diff Viewer for Termux\n");
    printf("Connect from Neovim using :SeeCodeDiff command\n");
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    int verbose = 0;
    int debug = 0;
    int check_only = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            debug = 1;
        } else if (strcmp(argv[i], "--check-deps") == 0) {
            check_only = 1;
        } else {
            printf("Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Initialize logging
    logger_init(verbose, debug);
    
    log_info("Starting see_code application");
    log_info("Version: %s", APP_VERSION);
    log_info("Build: %s %s", __DATE__, __TIME__);
    
    // Check system dependencies
    log_info("Checking system dependencies...");
    if (!deps_check_all()) {
        log_error("Dependency check failed");
        if (check_only) {
            return 1;
        }
        log_error("Continuing anyway, but functionality may be limited");
    } else {
        log_info("All dependencies satisfied");
        if (check_only) {
            printf("All dependencies satisfied\n");
            return 0;
        }
    }
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize application
    AppConfig config = {
        .socket_path = SOCKET_PATH,
        .window_width = 1080,
        .window_height = 2400,
        .landscape_mode = 1,
        .verbose = verbose,
        .debug = debug
    };
    
    if (!app_init(&config)) {
        log_error("Failed to initialize application");
        return 1;
    }
    
    log_info("Application initialized successfully");
    log_info("Listening for connections from Neovim...");
    
    // Main event loop
    while (g_running) {
        if (!app_update()) {
            log_error("Application update failed");
            break;
        }
        
        // Small sleep to prevent busy waiting
        usleep(16666); // ~60 FPS
    }
    
    log_info("Shutting down application");
    app_cleanup();
    logger_cleanup();
    
    return 0;
}
