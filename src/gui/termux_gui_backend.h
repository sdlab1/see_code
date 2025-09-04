// src/gui/termux_gui_backend.h
#ifndef SEE_CODE_TERMUX_GUI_BACKEND_H
#define SEE_CODE_TERMUX_GUI_BACKEND_H

#include "see_code/data/diff_data.h"
#include <stddef.h>

// Opaque pointer to hide termux-gui-c details
typedef struct TermuxGUIBackend TermuxGUIBackend;

// Backend functions
TermuxGUIBackend* termux_gui_backend_create(void);
void termux_gui_backend_destroy(TermuxGUIBackend* backend);
int termux_gui_backend_init(TermuxGUIBackend* backend);
void termux_gui_backend_render_diff(TermuxGUIBackend* backend, const DiffData* data);
void termux_gui_backend_handle_events(TermuxGUIBackend* backend);
int termux_gui_backend_is_available(void); // Check if termux-gui-c library can be loaded

#endif // SEE_CODE_TERMUX_GUI_BACKEND_H
