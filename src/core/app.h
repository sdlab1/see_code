// src/core/app.h
#ifndef SEE_CODE_APP_H
#define SEE_CODE_APP_H

#include "see_code/core/config.h"
#include "see_code/data/diff_data.h"

// Определения версии и времени компиляции
#ifndef APP_VERSION
#define APP_VERSION "0.1.0-dev"
#endif

// Application initialization and management
int app_init(const AppConfig* config);
void app_cleanup(void);

// Application state queries
int app_is_running(void);
void app_request_exit(void);

// Main loop functions
void app_update(float delta_time);
int app_render(void);

// Event handling
void app_handle_key(int key_code);
void app_handle_touch(float x, float y);
void app_handle_scroll(float delta_y);

// Data functions
void app_set_diff_data(const DiffData* data);
const DiffData* app_get_diff_data(void);

// Стандартная функция, но недостающая
int app_update(void);
void app_shutdown(void);
void app_handle_resize(int width, int height);
const AppConfig* app_get_config(void);
void app_handle_scroll(float dx, float dy);

// Функция для получения времени (используется в widgets.c)
unsigned long long app_get_time_millis(void);

#endif // SEE_CODE_APP_H
