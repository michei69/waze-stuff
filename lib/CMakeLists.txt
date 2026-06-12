cmake_minimum_required(VERSION 3.14)
project(waze-tools C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -O2 -g")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
    renderer/main.c
    ${SHARED_SOURCES}
    renderer/utils.h
)
target_include_directories(waze-renderer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                             ${CMAKE_CURRENT_SOURCE_DIR}/renderer)
target_link_libraries(waze-renderer PRIVATE raylib ZLIB::ZLIB)

# ============================================================================
#  Target 3: geojson-parser (GeoJSON converter)
# ============================================================================

# ---- cJSON via FetchContent ----
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
set(ENABLE_CJSON_TEST OFF CACHE BOOL "" FORCE)
set(ENABLE_CJSON_UTILS OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_AND_STATIC_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    cjson
    GIT_REPOSITORY "https://github.com/DaveGamble/cJSON.git"
    GIT_TAG "v1.7.18"
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(cjson)

unset(CMAKE_POLICY_VERSION_MINIMUM CACHE)

add_executable(geojson-parser
    geojson/main.c
    ${SHARED_SOURCES}
)
target_include_directories(geojson-parser PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                   ${cjson_SOURCE_DIR})
target_link_libraries(geojson-parser PRIVATE cjson ZLIB::ZLIB)

# ============================================================================
#  Target 4: json-to-wzdf (JSON tile data -> WZT binary)
# ============================================================================
add_executable(json-to-wzdf
    json-to-wzdf/main.c
)
target_include_directories(json-to-wzdf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
                                                 ${cjson_SOURCE_DIR})
target_link_libraries(json-to-wzdf PRIVATE cjson)
