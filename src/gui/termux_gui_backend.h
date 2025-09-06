// src/gui/termux_gui_backend.h
#ifndef SEE_CODE_TERMUX_GUI_BACKEND_H
#define SEE_CODE_TERMUX_BACKEND_H

#include <stddef.h> // for size_t
#include "see_code/data/diff_data.h" // Для DiffData

// Opaque pointer to hide internal TermuxGUIBackend structure
typedef struct TermuxGUIBackend TermuxGUIBackend;

// Function declarations for Termux GUI Backend

/**
 * @brief Checks if the Termux GUI library is available.
 * 
 * This function attempts to dynamically load the Termux GUI library
 * (libtermux-gui.so or libtermux-gui-c.so) to determine if the fallback
 * rendering path is possible.
 * 
 * @return 1 if the library is available, 0 otherwise.
 */
int termux_gui_backend_is_available(void);

/**
 * @brief Creates a new TermuxGUIBackend instance.
 * 
 * This function initializes the internal structure but does not yet
 * establish a connection or create an activity. It prepares the backend
 * for initialization.
 * 
 * @return A pointer to the newly created TermuxGUIBackend, or NULL on failure.
 */
TermuxGUIBackend* termux_gui_backend_create(void);

/**
 * @brief Initializes the TermuxGUIBackend.
 * 
 * This function establishes a connection to the Termux GUI service and
 * creates a new activity (window) for the application.
 * 
 * @param backend A pointer to the TermuxGUIBackend to initialize.
 * @return 1 on success, 0 on failure.
 */
int termux_gui_backend_init(TermuxGUIBackend* backend);

/**
 * @brief Renders diff data using the Termux GUI backend.
 * 
 * This function takes diff data and creates/updates the corresponding
 * UI elements (TextViews, etc.) in the Termux GUI activity.
 * 
 * @param backend A pointer to the initialized TermuxGUIBackend.
 * @param data A pointer to the DiffData to render.
 */
void termux_gui_backend_render_diff(TermuxGUIBackend* backend, const DiffData* data);

/**
 * @brief Handles events from the Termux GUI backend.
 * 
 * This function polls for events from the Termux GUI service and
 * processes them (e.g., touch events, key presses). It should be
 * called regularly in the application's main loop.
 * 
 * @param backend A pointer to the initialized TermuxGUIBackend.
 * @param timeout_ms The maximum time to block waiting for an event, in milliseconds.
 *                   Use 0 for non-blocking, -1 for indefinite blocking.
 * @return 1 if an event was handled, 0 if no event was available or an error occurred.
 */
int termux_gui_backend_handle_events(TermuxGUIBackend* backend, int timeout_ms);

/**
 * @brief Destroys a TermuxGUIBackend instance.
 * 
 * This function cleans up all resources associated with the backend,
 * including closing the connection and destroying the activity.
 * 
 * @param backend A pointer to the TermuxGUIBackend to destroy.
 *                This pointer becomes invalid after the call.
 */
void termux_gui_backend_destroy(TermuxGUIBackend* backend);

// --- Функции для рендеринга виджетов через Termux-GUI (New) ---
// Объявления функций для создания и управления виджетами в Termux-GUI

/**
 * @brief Рендерит текстовое поле ввода через Termux-GUI.
 * 
 * @param backend Указатель на активный TermuxGUIBackend.
 * @param input Указатель на состояние текстового поля.
 * @return 1 при успехе, 0 при ошибке.
 */
int termux_gui_backend_render_text_input(TermuxGUIBackend* backend, const TextInputState* input);

/**
 * @brief Рендерит кнопку через Termux-GUI.
 * 
 * @param backend Указатель на активный TermuxGUIBackend.
 * @param button Указатель на состояние кнопки.
 * @return 1 при успехе, 0 при ошибке.
 */
int termux_gui_backend_render_button(TermuxGUIBackend* backend, const ButtonState* button);

#endif // SEE_CODE_TERMUX_GUI_BACKEND_H
