// src/gui/termux_gui_backend.h
#ifndef SEE_CODE_TERMUX_GUI_BACKEND_H
#define SEE_CODE_TERMUX_GUI_BACKEND_H

// Forward declarations
typedef struct TermuxGUIBackend TermuxGUIBackend;
typedef struct DiffData DiffData;
typedef struct TextInputState TextInputState;
typedef struct ButtonState ButtonState;

/**
 * @brief Checks if Termux-GUI library is available at runtime.
 *
 * This function attempts to dynamically load the termux-gui-c library
 * and checks for essential symbols. It can be called multiple times safely.
 *
 * @return 1 if the library is available and functional, 0 otherwise.
 */
int termux_gui_backend_is_available(void);

/**
 * @brief Creates a new TermuxGUIBackend instance.
 *
 * Allocates memory and initializes a backend instance. The backend
 * is not immediately ready for use until termux_gui_backend_init() is called.
 *
 * @return A pointer to the new backend instance, or NULL on failure.
 */
TermuxGUIBackend* termux_gui_backend_create(void);

/**
 * @brief Initializes the TermuxGUI backend.
 *
 * Establishes connection to the Termux:GUI service and creates an activity.
 * This function must be called before rendering operations.
 *
 * @param backend The backend instance to initialize.
 * @return 1 on success, 0 on failure.
 */
int termux_gui_backend_init(TermuxGUIBackend* backend);

/**
 * @brief Destroys the TermuxGUI backend and frees resources.
 *
 * Properly closes connections, destroys activities, and frees memory.
 * The backend pointer becomes invalid after this call.
 *
 * @param backend The backend instance to destroy. Can be NULL.
 */
void termux_gui_backend_destroy(TermuxGUIBackend* backend);

/**
 * @brief Renders diff data using the Termux-GUI backend.
 *
 * Creates native Android views to display the diff data. This includes
 * file headers, hunk headers (as buttons), and individual diff lines
 * with appropriate colors.
 *
 * @param backend The initialized backend instance.
 * @param data The diff data to render. Can be NULL (will show empty state).
 */
void termux_gui_backend_render_diff(TermuxGUIBackend* backend, const DiffData* data);

/**
 * @brief Renders a text input widget using Termux-GUI.
 *
 * Creates an EditText view with the current text, position, and styling
 * from the input state.
 *
 * @param backend The initialized backend instance.
 * @param input The text input state to render.
 * @return 1 on success, 0 on failure.
 */
int termux_gui_backend_render_text_input(TermuxGUIBackend* backend, const TextInputState* input);

/**
 * @brief Renders a button widget using Termux-GUI.
 *
 * Creates a Button view with the label, position, and styling
 * from the button state.
 *
 * @param backend The initialized backend instance.
 * @param button The button state to render.
 * @return 1 on success, 0 on failure.
 */
int termux_gui_backend_render_button(TermuxGUIBackend* backend, const ButtonState* button);

/**
 * @brief Updates an existing text input widget.
 *
 * In Termux-GUI, this typically requires recreating the view.
 *
 * @param backend The initialized backend instance.
 * @param input The updated text input state.
 * @return 1 on success, 0 on failure.
 */
int termux_gui_backend_update_text_input(TermuxGUIBackend* backend, const TextInputState* input);

/**
 * @brief Updates an existing button widget.
 *
 * In Termux-GUI, this typically requires recreating the view.
 *
 * @param backend The initialized backend instance.
 * @param button The updated button state.
 * @return 1 on success, 0 on failure.
 */
int termux_gui_backend_update_button(TermuxGUIBackend* backend, const ButtonState* button);

/**
 * @brief Handles events from the Termux-GUI system.
 *
 * Processes user interactions like clicks, text input, etc.
 * This function should be called regularly or in a dedicated thread.
 *
 * @param backend The initialized backend instance.
 */
void termux_gui_backend_handle_events(TermuxGUIBackend* backend);

#endif // SEE_CODE_TERMUX_GUI_BACKEND_H
