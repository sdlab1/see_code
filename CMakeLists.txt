# CMakeLists.txt (в корне проекта)
cmake_minimum_required(VERSION 3.10)
project(see_code C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Определяем пути к исходникам
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Добавляем директории для поиска заголовочных файлов
include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/core
    ${SRC_DIR}/data
    ${SRC_DIR}/gui
    ${SRC_DIR}/network
    ${SRC_DIR}/utils
)

# Добавляем поддиректорию src, где находятся CMakeLists.txt для библиотек
add_subdirectory(src)

# Создаем исполняемый файл
add_executable(see_code
    src/core/main.c
)

# Линкуем необходимые библиотеки
# Убрана линковка с cjson
target_link_libraries(see_code
    see_code_core
    see_code_gui
    see_code_network
    see_code_data
    see_code_utils
    GLESv2
    EGL
    # cjson REMOVED
    pthread
    dl # Для dlopen в deps_check
)

# Устанавливаем исполняемый файл в префикс/bin
install(TARGETS see_code DESTINATION bin)

# Устанавливаем плагин Neovim
install(
    FILES plugin/see_code.lua
    DESTINATION share/nvim/site/plugin
    RENAME see_code.lua
)
