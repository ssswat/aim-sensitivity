cmake_minimum_required(VERSION 3.26)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT DEFINED CMAKE_BUILD_TYPE AND NOT DEFINED ENV{CMAKE_BUILD_TYPE})
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type")
endif ()

project(aim_sensitivity VERSION 1.3.0 LANGUAGES CXX)

# The Dusklight version to build against: a release tag (e.g. "v2.0.3") or a commit SHA.
# Native mods that hook game code must be built for the SAME Dusklight version you play on.
if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "v2.0.3")
endif ()
# Fetches the Dusklight sources into ./dusklight and provides DUSKLIGHT_DIR.
# Use -DDUSKLIGHT_DIR= to point at an existing Dusklight checkout.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDusklight.cmake")
add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)

configure_file(mod.json.in "${CMAKE_CURRENT_BINARY_DIR}/mod.json" @ONLY NEWLINE_STYLE LF)

# MSVC reports __cplusplus as 199711 unless told otherwise, which makes the game headers
# (dolphin/types.h) define `nullptr` as a macro and break the standard library. Report the real value.
if (MSVC)
    add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>")
endif ()

add_mod(aim_sensitivity
        FEATURES game              # needed to hook game code
        SOURCES src/mod.cpp
        MOD_JSON "${CMAKE_CURRENT_BINARY_DIR}/mod.json"
        RES_DIR res
)
