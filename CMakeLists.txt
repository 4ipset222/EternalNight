cmake_minimum_required(VERSION 3.10)
project(EternalNight LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(GLEW REQUIRED)
find_package(PNG REQUIRED)
find_package(Freetype REQUIRED)

add_library(FORGE STATIC
    src/engine/input.c
    src/engine/renderer.c
    src/engine/camera.c
    src/engine/forge.c
    src/engine/forgesystem.c
    src/engine/rendertexture.c
    src/engine/window.c
    src/engine/timer.c
    src/engine/worldgen.c
)

target_include_directories(FORGE PUBLIC
    src
)

target_link_libraries(FORGE PUBLIC
    OpenMP::OpenMP_C
    SDL2::SDL2
    GLEW::GLEW
    PNG::PNG
    Freetype::Freetype
)

add_executable(EternalNight
    src/inventory.cpp
    src/inventory.h
    src/main.cpp
    src/player.cpp
    src/player.h
    src/world.cpp
    src/world.h
)

target_link_libraries(EternalNight PRIVATE
    FORGE
    SDL2::SDL2main
)
