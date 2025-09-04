# CMakeLists.txt (в корне проекта)
cmake_minimum_required(VERSION 3.10)
project(see_code C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Определяем пути к исходникам
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Добавляем директории для поиска заголовочных файлов
# ВАЖНО: Указываем путь к src/, так как заголовки там (#include "see_code/core/app.h")
include_directories(${SRC_DIR})

# --- Все библиотеки и исполняемый файл определяются прямо здесь ---

# Core library
add_library(see_code_core
    ${SRC_DIR}/core/app.c
)

# GUI library
add_library(see_code_gui
    ${SRC_DIR}/gui/renderer.c
    ${SRC_DIR}/gui/ui_manager.c
    ${SRC_DIR}/gui/termux_gui_backend.c
)
target_link_libraries(see_code_gui
    GLESv2
    EGL
)

# Network library
add_library(see_code_network
    ${SRC_DIR}/network/socket_server.c
)
target_link_libraries(see_code_network
    pthread
)

# Data library
add_library(see_code_data
    ${SRC_DIR}/data/diff_data.c
)
# target_link_libraries(see_code_data cjson) # Убрано, так как отказались от cJSON

# Utils library
add_library(see_code_utils
    ${SRC_DIR}/utils/logger.c
    ${SRC_DIR}/utils/deps_check.c
)
target_link_libraries(see_code_utils
    dl
)

# --- Конец определения библиотек ---

# Создаем исполняемый файл
add_executable(see_code
    ${SRC_DIR}/core/main.c
)

# Линкуем необходимые библиотеки
target_link_libraries(see_code
    see_code_core
    see_code_gui
    see_code_network
    see_code_data
    see_code_utils
    GLESv2
    EGL
    pthread
    dl
    # cjson REMOVED
)

# Устанавливаем исполняемый файл в префикс/bin
install(TARGETS see_code DESTINATION bin)

# Устанавливаем плагин Neovim
install(
    FILES plugin/see_code.lua
    DESTINATION share/nvim/site/plugin
    RENAME see_code.lua
)
