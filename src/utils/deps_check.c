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
    // Check if we're running in Termux
    if (access("/data/data/com.termux", F_OK) != 0) {
        log_error("Not running in Termux environment");
        log_error("SOLUTION: This application must be run inside Termux");
        return 0;
    }
    // Check PREFIX environment variable
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
    // Check if Termux:GUI app is installed
    if (access("/data/data/com.termux.gui", F_OK) != 0) {
        log_error("Termux:GUI app not installed");
        log_error("SOLUTION: Install Termux:GUI from F-Droid or GitHub releases");
        log_error("URL: https://github.com/tareksander/termux-gui/releases  ");
        return 0;
    }
    // Check if GUI service files exist
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
        log_error("OpenGL ES 2.0 library not found: %s", dlerror());
        log_error("SOLUTION: Install graphics libraries: pkg install mesa");
        return 0;
    }
    // Check for essential GLES2 functions
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
        // Try alternative names
        handle = dlopen("libfreetype.so.6", RTLD_LAZY);
        if (!handle) {
            log_warn("FreeType library not found, will try TrueType fallback");
            log_warn("SOLUTION: Install FreeType: pkg install freetype");
            return 0;
        }
    }
    // Check for essential FreeType functions
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
int deps_check_cjson(void) {
    void* handle = dlopen("libcjson.so", RTLD_LAZY);
    if (!handle) {
        // Try alternative names
        handle = dlopen("libcjson.so.1", RTLD_LAZY);
        if (!handle) {
            log_error("cJSON library not found: %s", dlerror());
            log_error("SOLUTION: Install cJSON: pkg install cjson");
            return 0;
        }
    }
    // Check for essential cJSON functions
    void* cJSON_Parse = dlsym(handle, "cJSON_Parse");
    void* cJSON_Delete = dlsym(handle, "cJSON_Delete");
    if (!cJSON_Parse || !cJSON_Delete) {
        log_error("Essential cJSON functions not found");
        dlclose(handle);
        return 0;
    }
    dlclose(handle);
    log_info("cJSON library: OK");
    return 1;
}
int deps_check_pthread(void) {
    void* handle = dlopen("libpthread.so", RTLD_LAZY);
    if (!handle) {
        // pthread is usually part of libc on Android
        handle = dlopen("libc.so", RTLD_LAZY);
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
// --- NEW FUNCTION ---
int deps_check_termux_gui_c(void) {
    void* handle = dlopen("libtermux-gui.so", RTLD_LAZY);
    if (!handle) {
        // Try alternative names
        handle = dlopen("libtermux-gui-c.so", RTLD_LAZY);
        if (!handle) {
            log_warn("termux-gui-c library not found, fallback will not be available");
            log_warn("SOLUTION: Install termux-gui-c if you want Termux GUI fallback: pkg install termux-gui-c");
            return 0; // Not critical, just a fallback
        }
    }
    // Check for essential termux-gui-c functions
    void* tgui_create = dlsym(handle, "tgui_create");
    void* tgui_destroy = dlsym(handle, "tgui_destroy");
    if (!tgui_create || !tgui_destroy) {
        log_error("Essential termux-gui-c functions not found");
        dlclose(handle);
        return 0;
    }
    dlclose(handle);
    log_info("termux-gui-c library: OK (Fallback available)");
    return 1;
}
int deps_check_libraries(void) {
    log_info("Checking required libraries...");
    int all_ok = 1;
    if (!deps_check_gles2()) all_ok = 0;
    if (!deps_check_cjson()) all_ok = 0;
    if (!deps_check_pthread()) all_ok = 0;
    // FreeType is optional (fallback to TrueType)
    deps_check_freetype();
    // Check for Termux GUI C library (optional fallback)
    deps_check_termux_gui_c(); 
    return all_ok;
}
int deps_check_fonts(void) {
    log_info("Checking font availability...");
    // Check FreeType fonts
    if (access(FREETYPE_FONT_PATH, R_OK) == 0) {
        log_info("Primary font found: %s", FREETYPE_FONT_PATH);
        return 1;
    }
    // Check TrueType fallback
    if (access(TRUETYPE_FONT_PATH, R_OK) == 0) {
        log_info("Fallback font found: %s", TRUETYPE_FONT_PATH);
        return 1;
    }
    // Check Termux fonts
    if (access(FALLBACK_FONT_PATH, R_OK) == 0) {
        log_info("Termux font found: %s", FALLBACK_FONT_PATH);
        return 1;
    }
    log_warn("No suitable fonts found");
    log_warn("SOLUTION: Install fonts: pkg install fontconfig ttf-dejavu");
    return 0;
}
int deps_check_permissions(void) {
    log_info("Checking permissions...");
    // Check socket directory write permission
    char socket_dir[256];
    strncpy(socket_dir, SOCKET_PATH, sizeof(socket_dir) - 1);
    char* last_slash = strrchr(socket_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (access(socket_dir, W_OK) != 0) {
            log_error("Cannot write to socket directory: %s", socket_dir);
            log_error("SOLUTION: Check directory permissions");
            return 0;
        }
    }
    // Check temp directory access
    if (access("/data/data/com.termux/files/usr/tmp", W_OK) != 0) {
        log_warn("Cannot write to temp directory");
        log_warn("SOLUTION: Create temp directory: mkdir -p $PREFIX/tmp");
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
    // Font check is not critical (we have fallbacks)
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
    printf("=== see_code Dependency Report ===
");
    printf("Environment: %s
", deps_check_termux_environment() ? "✓ OK" : "✗ FAIL");
    printf("Termux:GUI:  %s
", deps_check_termux_gui_app() ? "✓ OK" : "✗ FAIL");
    printf("OpenGL ES2:  %s
", deps_check_gles2() ? "✓ OK" : "✗ FAIL");
    printf("cJSON:       %s
", deps_check_cjson() ? "✓ OK" : "✗ FAIL");
    printf("pthread:     %s
", deps_check_pthread() ? "✓ OK" : "✗ FAIL");
    printf("FreeType:    %s
", deps_check_freetype() ? "✓ OK" : "⚠ WARN");
    printf("Fonts:       %s
", deps_check_fonts() ? "✓ OK" : "⚠ WARN");
    printf("Permissions: %s
", deps_check_permissions() ? "✓ OK" : "✗ FAIL");
    printf("
Overall: %s
", g_all_deps_ok ? "PASS" : "FAIL");
}
const char* deps_get_missing_solution(const char* component) {
    if (strcmp(component, "gles2") == 0) {
        return "Install graphics libraries: pkg install mesa";
    } else if (strcmp(component, "freetype") == 0) {
        return "Install FreeType: pkg install freetype";
    } else if (strcmp(component, "cjson") == 0) {
        return "Install cJSON: pkg install cjson";
    } else if (strcmp(component, "termux-gui") == 0) {
        return "Install Termux:GUI from F-Droid or GitHub releases";
    }
    return "No specific solution available";
}
