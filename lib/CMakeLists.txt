cmake_minimum_required(VERSION 3.14)
project(waze-tools C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")

# ---- Shared library sources ----
set(SHARED_SOURCES
    wzt.c
    wzdf.c
    wzm.c
)

# ---- zlib (required for WZDF decompression) ----
find_package(ZLIB REQUIRED)

# ============================================================================
#  Target 1: waze-parser (CLI)
# ============================================================================
add_executable(waze-parser
    cli/main.c
    ${SHARED_SOURCES}
)
target_include_directories(waze-parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(waze-parser PRIVATE ZLIB::ZLIB)

# ============================================================================
#  Target 2: waze-renderer (raylib GUI)
# ============================================================================

# ---- Raylib via FetchContent ----
include(FetchContent)
set(FETCHCONTENT_QUIET TRUE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(RAYLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_WAYLAND ON CACHE BOOL "" FORCE)
set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY "https://github.com/raysan5/raylib.git"
    GIT_TAG "master"
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(raylib)

add_executable(waze-renderer
    renderer.c
    ${SHARED_SOURCES}
        utils.h
)
target_include_directories(waze-renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(waze-renderer PRIVATE raylib ZLIB::ZLIB)
