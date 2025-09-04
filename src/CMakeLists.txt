# Core library
add_library(see_code_core
    core/app.c
)

target_include_directories(see_code_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# GUI library
add_library(see_code_gui
    gui/renderer.c
    gui/ui_manager.c
)

target_include_directories(see_code_gui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(see_code_gui
    GLESv2
    EGL
)

# Network library
add_library(see_code_network
    network/socket_server.c
)

target_include_directories(see_code_network PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(see_code_network
    pthread
)

# Data library
add_library(see_code_data
    data/diff_data.c
)

target_include_directories(see_code_data PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(see_code_data
    cjson
)

# Utils library
add_library(see_code_utils
    utils/logger.c
    utils/deps_check.c
)

target_include_directories(see_code_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(see_code_utils
    dl
)
