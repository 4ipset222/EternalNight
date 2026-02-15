cmake_minimum_required(VERSION 3.10)
project(EternalNight LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(PNG REQUIRED)
find_package(Freetype REQUIRED)

add_library(forge SHARED
    src/engine/input.c
    src/engine/renderer.c
    src/engine/camera.c
    src/engine/forge.c
    src/engine/forgesystem.c
    src/engine/rendertexture.c
    src/engine/window.c
    src/engine/timer.c
    src/engine/worldgen.c
    src/engine/storage.c
    src/engine/renderer_d2d.cpp
)

if(WIN32)
    set_target_properties(forge PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
endif()

target_include_directories(forge PUBLIC
    src
)

target_link_libraries(forge PUBLIC
    OpenMP::OpenMP_C
    SDL2::SDL2
    GLEW::GLEW
    PNG::PNG
    Freetype::Freetype
)

if(WIN32)
    target_link_libraries(forge PUBLIC
        d2d1
        dwrite
        dxguid
        windowscodecs
        ole32
    )
endif()

add_library(NETWORK STATIC
    src/net/client.c
    src/net/client.h
    src/net/net.c
    src/net/net.h
    src/net/protocol.h
    src/net/server.c
    src/net/server.h
)

target_include_directories(NETWORK PUBLIC
    src
)

target_link_libraries(NETWORK PUBLIC
    ws2_32
)

add_executable(EternalNight
    src/inventory.cpp
    src/inventory.h
    src/main.cpp
    src/player.cpp
    src/player.h
    src/game_input.cpp
    src/game_input.h
    src/game_logic.cpp
    src/game_logic.h
    src/game_render.cpp
    src/game_render.h
    src/multiplayer.cpp
    src/multiplayer.h
    src/ui/eternal_ui.cpp
    src/ui/eternal_ui.h
    src/ui/imgui_lite.cpp
    src/ui/imgui_lite.h
    src/ui/main_menu.cpp
    src/ui/main_menu.h
    src/ui/menu_background.cpp
    src/ui/menu_background.h
    src/ui/multiplayer_ui.cpp
    src/ui/multiplayer_ui.h
    src/multiplayer_types.h
    src/world.cpp
    src/world.h
)

target_link_libraries(EternalNight PRIVATE
    forge
    NETWORK
    SDL2::SDL2main
)

add_executable(EternalNight-srv
    src/server_main.cpp
    src/world.cpp
    src/world.h
)

target_link_libraries(EternalNight-srv PRIVATE
    forge
    NETWORK
)

target_compile_definitions(EternalNight-srv PRIVATE SDL_MAIN_HANDLED)
