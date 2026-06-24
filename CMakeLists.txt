cmake_minimum_required(VERSION 3.21)

project(LazyCMake
    VERSION 0.1.0
    DESCRIPTION "lazygit, but for CMake"
    LANGUAGES CXX
)

# --- Global project settings -------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LAZYCMAKE_BUILD_TESTS "Build the Catch2 test suite" ON)
option(LAZYCMAKE_BUILD_EXAMPLES "Build the examples" ON)
option(LAZYCMAKE_BUILD_TUI "Build the FTXUI-based TUI application (off during early Core-only phases)" OFF)

# --- Dependencies (FetchContent) -----------------------------------------
# Pinned tags throughout: LazyCMake's own build is a working example of the
# dependency story it gives to end users (docs/DESIGN.md §13).

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 11.0.2
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.6.0
)

# spdlog can use its own bundled fmt; we want it to use the one above to
# avoid pulling two copies of fmt into the build.
set(SPDLOG_FMT_EXTERNAL ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(nlohmann_json fmt spdlog)

if(LAZYCMAKE_BUILD_TESTS)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
endif()

# Phase 5+ adds FTXUI, toml++, efsw, and reproc here behind
# LAZYCMAKE_BUILD_TUI, once the TUI layer (docs/DESIGN.md Phase 5) starts.
# Kept out for now so that Phase 0/1 (Core layer only) has the smallest
# possible dependency graph and fastest CI iteration.

# --- lazycmake_core library ------------------------------------------------
# Pure domain logic: Project/Target/DependencySpec models, validation,
# manifest load/save. No FTXUI dependency anywhere in this target — see
# the lint check below, which enforces docs/DESIGN.md §4's TUI boundary.

add_library(lazycmake_core STATIC
    src/core/types.cpp
    src/core/project.cpp
    src/core/target_validator.cpp
    src/core/project_repository.cpp
    src/cmake_gen/generated_file_set.cpp
    src/cmake_gen/cmake_file.cpp
    src/cmake_gen/cmake_printer.cpp
    src/cmake_gen/modern_cmake_generator.cpp
    src/config/config_types.cpp
    src/config/settings_manager.cpp
    src/config/keymap_manager.cpp
    src/config/theme_manager.cpp
    src/config/template_manager.cpp
    src/events/event_bus.cpp
    src/build/build_backend.cpp
    src/build/build_manager.cpp
    src/build/run_manager.cpp
)

target_include_directories(lazycmake_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(lazycmake_core
    PUBLIC
        nlohmann_json::nlohmann_json
    PRIVATE
        fmt::fmt
        spdlog::spdlog
)

target_compile_features(lazycmake_core PUBLIC cxx_std_20)

if(MSVC)
    target_compile_options(lazycmake_core PRIVATE /W4)
else()
    target_compile_options(lazycmake_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- FTXUI boundary lint --------------------------------------------------
# docs/DESIGN.md §4: src/tui/ is the only directory permitted to #include
# any FTXUI header. This keeps Core testable without a terminal and keeps
# the dependency graph honest. Cheap grep-based check, run at configure
# time so a violation fails fast rather than silently compiling once
# FTXUI is added to the build in a later phase.
file(GLOB_RECURSE LAZYCMAKE_NON_TUI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cmake_gen/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/build/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/config/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/plugins/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fswatch/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/events/*.cpp
)
foreach(_lazycmake_src_file ${LAZYCMAKE_NON_TUI_SOURCES})
    file(STRINGS ${_lazycmake_src_file} _lazycmake_ftxui_hits REGEX "ftxui/")
    if(_lazycmake_ftxui_hits)
        message(FATAL_ERROR
            "FTXUI boundary violation: '${_lazycmake_src_file}' includes an "
            "ftxui/ header. Only files under src/tui/ may depend on FTXUI "
            "(see docs/DESIGN.md section 4).")
    endif()
endforeach()

# --- Tests -----------------------------------------------------------------

if(LAZYCMAKE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LAZYCMAKE_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
