// src/utils/deps_check.c
#include "see_code/utils/deps_check.h"
#include "see_code/utils/logger.h"
#include "see_code/core/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>

static int g_deps_checked = 0;
static int g_all_deps_ok = 0;

int deps_check_termux_environment(void) {
    if (access("/data/data/com.termux", F_OK) != 0) {
        log_error("Not running in Termux environment");
        log_error("SOLUTION: This application must be run inside Termux");
        return 0;
    }

    const char* prefix = getenv("PREFIX");
    if (!prefix || strstr(prefix, "com.termux") == NULL) {
        log_warn("PREFIX environment variable not set correctly");
        log_warn("SOLUTION: Source Termux environment: source $PREFIX/etc/profile");
        return 0;
    }
    log_info("Termux environment: OK");
    return 1;
}

int deps_check_termux_gui_app(void) {
    if (access("/data/data/com.termux.gui", F_OK) != 0) {
        log_error("Termux:GUI app not installed");
        log_error("SOLUTION: Install Termux:GUI from F-Droid or GitHub releases");
        return 0;
    }

    if (access("/data/data/com.termux.gui/files/termux-gui", F_OK) != 0) {
        log_warn("Termux:GUI service not found");
        log_warn("SOLUTION: Ensure Termux:GUI app is properly installed and running");
        return 0;
    }
    log_info("Termux:GUI app: OK");
    return 1;
}

int deps_check_gles2(void) {
    void* handle = dlopen("libGLESv2.so", RTLD_LAZY);
    if (!handle) {
        handle = dlopen("libGLESv2.so.2", RTLD_LAZY); // Try common variant
        if (!handle) {
             log_error("OpenGL ES 2.0 library not found: %s", dlerror());
             log_error("SOLUTION: Install graphics libraries: pkg install mesa");
             return 0;
        }
    }

    void* glCreateProgram = dlsym(handle, "glCreateProgram");
    void* glCreateShader = dlsym(handle, "glCreateShader");
    if (!glCreateProgram || !glCreateShader) {
        log_error("Essential GLES2 functions not found");
        dlclose(handle);
        return 0;
    }
    dlclose(handle);
    log_info("OpenGL ES 2.0: OK");
    return 1;
}

int deps_check_freetype(void) {
    void* handle = dlopen("libfreetype.so", RTLD_LAZY);
    if (!handle) {
        handle = dlopen("libfreetype.so.6", RTLD_LAZY); // Try common variant
        if (!handle) {
            log_warn("FreeType library not found, will try TrueType fallback");
            log_warn("SOLUTION: Install FreeType: pkg install freetype");
            return 0; // Return 0 even if it's a warn, as it's a specific check
        }
    }

    void* FT_Init_FreeType = dlsym(handle, "FT_Init_FreeType");
    void* FT_New_Face = dlsym(handle, "FT_New_Face");
    if (!FT_Init_FreeType || !FT_New_Face) {
        log_error("Essential FreeType functions not found");
        dlclose(handle);
        return 0;
    }
    dlclose(handle);
    log_info("FreeType library: OK");
    return 1;
}

// --- REMOVED FUNCTION: deps_check_cjson ---
// int deps_check_cjson(void) { ... }

int deps_check_pthread(void) {
    void* handle = dlopen("libpthread.so", RTLD_LAZY);
    if (!handle) {
        handle = dlopen("libc.so", RTLD_LAZY); // pthread is usually part of libc
        if (!handle) {
            log_error("pthread library not found");
            return 0;
        }
    }
    void* pthread_create = dlsym(handle, "pthread_create");
    void* pthread_mutex_init = dlsym(handle, "pthread_mutex_init");
    if (!pthread_create || !pthread_mutex_init) {
        log_error("Essential pthread functions not found");
        dlclose(handle);
        return 0;
    }
    dlclose(handle);
    log_info("pthread library: OK");
    return 1;
}

// --- NEW FUNCTION (остаётся без изменений, если termux-gui-c всё ещё используется для fallback) ---
int deps_check_termux_gui_c(void) {
    void* handle = dlopen("libtermux-gui.so", RTLD_LAZY);
    if (!handle) {
        handle = dlopen("libtermux-gui-c.so", RTLD_LAZY); // Try alternative name
        if (!handle) {
            log_warn("termux-gui-c library not found, fallback will not be available");
            log_warn("SOLUTION: Install termux-gui-c if you want Termux GUI fallback: pkg install termux-gui-c");
            return 0; // Not critical for this specific check, but indicates fallback unavailability
        }
    }

    // Check for a few essential symbols to confirm it's the right library
    void* tgui_connection_create = dlsym(handle, "tgui_connection_create");
    void* tgui_activity_create = dlsym(handle, "tgui_activity_create");
    if (!tgui_connection_create || !tgui_activity_create) {
        log_error("Essential termux-gui-c functions not found");
        dlclose(handle);
        return 0;
    }
    // Note: We don't close the handle here as it might be used later.
    // The check is just for availability at this stage.
    // dlclose(handle);
    log_info("termux-gui-c library: OK (Fallback available)");
    return 1;
}

int deps_check_libraries(void) {
    log_info("Checking required libraries...");
    int all_ok = 1;
    if (!deps_check_gles2()) all_ok = 0;
    // if (!deps_check_cjson()) all_ok = 0; // REMOVED
    if (!deps_check_pthread()) all_ok = 0;

    // FreeType is optional (fallback to TrueType/system)
    deps_check_freetype(); // Log result, but don't fail the check

    // Check for Termux GUI C library (optional fallback)
    deps_check_termux_gui_c(); // Log result, but don't fail the check

    return all_ok;
}

int deps_check_fonts(void) {
    log_info("Checking font availability...");

    // 1. Check configured FreeType font path (from config.h)
    if (access(FREETYPE_FONT_PATH, R_OK) == 0) {
        log_info("Configured FreeType font found: %s", FREETYPE_FONT_PATH);
    } else {
        log_warn("Configured FreeType font not found: %s", FREETYPE_FONT_PATH);
    }

    // 2. Check configured TrueType fallback path (from config.h)
    if (access(TRUETYPE_FONT_PATH, R_OK) == 0) {
        log_info("Configured TrueType fallback font found: %s", TRUETYPE_FONT_PATH);
    } else {
        log_warn("Configured TrueType fallback font not found: %s", TRUETYPE_FONT_PATH);
    }

    // 3. Check configured Termux fallback path (from config.h)
    if (access(FALLBACK_FONT_PATH, R_OK) == 0) {
        log_info("Configured Termux fallback font found: %s", FALLBACK_FONT_PATH);
    } else {
        log_warn("Configured Termux fallback font not found: %s", FALLBACK_FONT_PATH);
    }

    // 4. Search for common system fonts dynamically (additional robustness)
    const char* common_font_dirs[] = {
        "/system/fonts",
        "/data/data/com.termux/files/usr/share/fonts",
        "/data/data/com.termux/files/usr/share/fonts/truetype",
        "/data/data/com.termux/files/usr/share/fonts/TTF",
        NULL
    };

    const char* common_font_names[] = {
        "Roboto-Regular.ttf", "Roboto.ttf",
        "DroidSansMono.ttf", "DroidSans.ttf",
        "LiberationMono-Regular.ttf", "LiberationMono.ttf",
        "DejaVuSansMono.ttf", "DejaVuSans.ttf",
        "NotoSans-Regular.ttf", "NotoSans.ttf",
        "Arial.ttf", "Helvetica.ttf",
        NULL
    };

    int fonts_found = 0;
    char potential_font_path[512];
    for (int i = 0; common_font_dirs[i] != NULL; i++) {
        if (access(common_font_dirs[i], F_OK) != 0) continue; // Skip if dir doesn't exist

        for (int j = 0; common_font_names[j] != NULL; j++) {
            snprintf(potential_font_path, sizeof(potential_font_path), "%s/%s", common_font_dirs[i], common_font_names[j]);
            if (access(potential_font_path, R_OK) == 0) {
                log_info("Dynamically found usable font: %s", potential_font_path);
                fonts_found++;
            }
        }
    }

    if (fonts_found > 0) {
        log_info("Found %d additional font(s) during dynamic search.", fonts_found);
    } else {
        log_info("No additional fonts found during dynamic search.");
    }

    // This function doesn't determine success/failure of the whole app,
    // it just reports what it found. The app logic (e.g., in app_init)
    // decides if this is sufficient based on its configuration or fallback strategy.
    return 1; // Always return 1 to indicate the check itself completed
}


int deps_check_permissions(void) {
    log_info("Checking permissions...");

    char socket_dir[256];
    strncpy(socket_dir, SOCKET_PATH, sizeof(socket_dir) - 1);
    char* last_slash = strrchr(socket_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (access(socket_dir, W_OK) != 0) {
            log_error("Cannot write to socket directory: %s", socket_dir);
            log_error("SOLUTION: Check directory permissions or run as correct user");
            return 0;
        }
    }

    if (access("/data/data/com.termux/files/usr/tmp", W_OK) != 0) {
        log_warn("Cannot write to standard temp directory");
        log_warn("SOLUTION: Ensure $PREFIX/tmp exists and is writable: mkdir -p $PREFIX/tmp");
    }
    log_info("Permissions: OK");
    return 1;
}

int deps_check_all(void) {
    if (g_deps_checked) {
        return g_all_deps_ok;
    }
    log_info("Performing comprehensive dependency check...");
    int all_ok = 1;
    if (!deps_check_termux_environment()) all_ok = 0;
    if (!deps_check_termux_gui_app()) all_ok = 0;
    if (!deps_check_libraries()) all_ok = 0;
    if (!deps_check_permissions()) all_ok = 0;

    // Font check is informational, logged by deps_check_fonts
    deps_check_fonts();

    g_deps_checked = 1;
    g_all_deps_ok = all_ok;
    if (all_ok) {
        log_info("All critical dependencies satisfied");
    } else {
        log_error("Some dependencies are missing or incorrect");
    }
    return all_ok;
}

void deps_print_report(void) {
    printf("=== see_code Dependency Report ===\n");
    printf("Environment: %s\n", deps_check_termux_environment() ? "✓ OK" : "✗ FAIL");
    printf("Termux:GUI:  %s\n", deps_check_termux_gui_app() ? "✓ OK" : "✗ FAIL");
    printf("OpenGL ES2:  %s\n", deps_check_gles2() ? "✓ OK" : "✗ FAIL");
    // printf("cJSON:       %s\n", deps_check_cjson() ? "✓ OK" : "✗ FAIL"); // REMOVED
    printf("pthread:     %s\n", deps_check_pthread() ? "✓ OK" : "✗ FAIL");
    printf("FreeType:    %s\n", deps_check_freetype() ? "✓ OK" : "⚠ WARN");
    // Run font check for report, result is logged inside
    deps_check_fonts();
    printf("Permissions: %s\n", deps_check_permissions() ? "✓ OK" : "✗ FAIL");
    printf("\nOverall: %s\n", g_all_deps_ok ? "PASS" : "FAIL");
}

const char* deps_get_missing_solution(const char* component) {
    if (strcmp(component, "gles2") == 0) {
        return "Install graphics libraries: pkg install mesa";
    } else if (strcmp(component, "freetype") == 0) {
        return "Install FreeType: pkg install freetype";
    // } else if (strcmp(component, "cjson") == 0) { // REMOVED
    //     return "Install cJSON: pkg install cjson";
    } else if (strcmp(component, "termux-gui") == 0) {
        return "Install Termux:GUI from F-Droid or GitHub releases";
    } else if (strcmp(component, "termux-gui-c") == 0) {
         return "Install termux-gui-c library: pkg install termux-gui-c";
    }
    return "No specific solution available";
}
