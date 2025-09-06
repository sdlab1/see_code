// src/gui/ui_manager.h
#ifndef SEE_CODE_UI_MANAGER_H
#define SEE_CODE_UI_MANAGER_H

#include "see_code/gui/renderer.h" // Для Renderer
#include "see_code/data/diff_data.h" // Для DiffData
#include "see_code/gui/termux_gui_backend.h" // Для TermuxGUIBackend

// --- Forward declarations for new widgets (New) ---
// Предполагаем, что определения находятся в see_code/gui/widgets.h
typedef struct TextInputState TextInputState;
typedef struct ButtonState ButtonState;

// Forward declaration
typedef struct UIManager UIManager;

// UI Manager functions
UIManager* ui_manager_create(Renderer* renderer);
void ui_manager_destroy(UIManager* ui_manager);
void ui_manager_set_diff_data(UIManager* ui_manager, DiffData* data);
void ui_manager_update_layout(UIManager* ui_manager, float scroll_y);
void ui_manager_render(UIManager* ui_manager);
int ui_manager_handle_touch(UIManager* ui_manager, float x, float y);
float ui_manager_get_content_height(UIManager* ui_manager);

// --- НОВАЯ ФУНКЦИЯ ДЛЯ ОБРАБОТКИ КЛАВИШ (New) ---
void ui_manager_handle_key(UIManager* ui_manager, int key_code);

// Renderer type management
typedef enum {
    RENDERER_TYPE_GLES2,
    RENDERER_TYPE_TERMUX_GUI
} RendererType;

void ui_manager_set_renderer_type(UIManager* ui_manager, RendererType type);
RendererType ui_manager_get_renderer_type(const UIManager* ui_manager);

#endif // SEE_CODE_UI_MANAGER_H
