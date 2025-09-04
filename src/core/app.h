#ifndef SEE_CODE_APP_H
#define SEE_CODE_APP_H

#include "see_code/core/config.h"

// Application initialization and management
int app_init(const AppConfig* config);
int app_update(void);
void app_shutdown(void);
void app_cleanup(void);

// Application state queries
int app_is_running(void);
const AppConfig* app_get_config(void);

// Event handling
void app_handle_touch(float x, float y);
void app_handle_scroll(float dx, float dy);
void app_handle_resize(int width, int height);

#endif // SEE_CODE_APP_H
