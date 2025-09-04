#ifndef SEE_CODE_UI_MANAGER_H
#define SEE_CODE_UI_MANAGER_H

#include "see_code/gui/renderer.h"
#include "see_code/data/diff_data.h"

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

#endif // SEE_CODE_UI_MANAGER_H
