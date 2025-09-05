# src/gui/CMakeLists.txt

# --- Добавляем подкаталог для модулей рендерера ---
add_subdirectory(renderer)
# --- Конец добавления подкаталога ---

# --- Основная библиотека GUI ---
add_library(see_code_gui
    renderer.c
    ui_manager.c
    termux_gui_backend.c
)

target_include_directories(see_code_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # Для gui/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../ # Для core/config.h, utils/logger.h и т.д.
)

# --- Линкуем необходимые библиотеки ---
target_link_libraries(see_code_gui
    # GLESv2 и EGL теперь линкуются внутри see_code_gl_components
    # GLESv2
    # EGL
    see_code_gl_components # Наш новый модуль с компонентами
    # freetype линкуется внутри see_code_gl_components
    # freetype
)
# --- Конец линковки ---
