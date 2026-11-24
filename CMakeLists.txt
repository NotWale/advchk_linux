cmake_minimum_required(VERSION 3.16)
project(RetroPort CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

add_executable(game
    src/main.cpp
    src/engine.cpp
    src/game_assets.cpp
    src/alien.cpp
    src/level.cpp
    src/gameplay.cpp
    src/gameplay_render.cpp
    src/bible_verses.cpp
    src/states.cpp
)
target_link_libraries(game PRIVATE SDL2::SDL2 SDL2_mixer::SDL2_mixer)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(game PRIVATE -Wall -Wextra)
endif()
