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

if(LAZYCMAKE_BUILD_TUI)
    FetchContent_Declare(
        ftxui
        GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
        GIT_TAG v5.0.0
    )
    FetchContent_MakeAvailable(ftxui)
endif()

# Phase 6+ adds toml++, efsw, and reproc here behind LAZYCMAKE_BUILD_TUI.

# --- lazycmake_core library ------------------------------------------------
# Pure domain logic: Project/Target/DependencySpec models, validation,
# manifest load/save. No FTXUI dependency anywhere in this target — see
# the lint check below, which enforces docs/DESIGN.md §4's TUI boundary.

# Find OpenSSL for SHA256 hashing (used by GeneratedFileLock).
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
    message(STATUS "OpenSSL not found; using bundled/simple hash fallback")
endif()

add_library(lazycmake_core STATIC
    # Core domain
    src/core/types.cpp
    src/core/project.cpp
    src/core/target_validator.cpp
    src/core/project_repository.cpp
    src/core/dependency_registry.cpp
    src/core/cmake_importer.cpp
    src/core/generated_file_lock.cpp
    # CMake generator
    src/cmake_gen/generated_file_set.cpp
    src/cmake_gen/cmake_file.cpp
    src/cmake_gen/cmake_printer.cpp
    src/cmake_gen/modern_cmake_generator.cpp
    # Configuration
    src/config/config_types.cpp
    src/config/settings_manager.cpp
    src/config/keymap_manager.cpp
    src/config/theme_manager.cpp
    src/config/template_manager.cpp
    # Events
    src/events/event_bus.cpp
    # Build engines
    src/build/build_backend.cpp
    src/build/build_manager.cpp
    src/build/run_manager.cpp
    src/build/diagnostic_parser.cpp
    # File watcher
    src/fswatch/file_watcher.cpp
    # Plugins
    src/plugins/plugin_host.cpp
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

if(OpenSSL_FOUND)
    target_link_libraries(lazycmake_core PRIVATE OpenSSL::Crypto)
    target_compile_definitions(lazycmake_core PRIVATE LAZYCMAKE_HAVE_OPENSSL=1)
else()
    target_compile_definitions(lazycmake_core PRIVATE LAZYCMAKE_HAVE_OPENSSL=0)
endif()

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
# Also check non-TUI headers in include/ (public API must not depend on FTXUI).
file(GLOB_RECURSE LAZYCMAKE_NON_TUI_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lazycmake/core/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lazycmake/cmake_gen/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lazycmake/build/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lazycmake/config/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lazycmake/plugins/*.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/lazycmake/events/*.hpp
)
foreach(_lazycmake_file ${LAZYCMAKE_NON_TUI_SOURCES} ${LAZYCMAKE_NON_TUI_HEADERS})
    file(STRINGS ${_lazycmake_file} _lazycmake_ftxui_hits REGEX "ftxui/")
    if(_lazycmake_ftxui_hits)
        message(FATAL_ERROR
            "FTXUI boundary violation: '${_lazycmake_file}' includes an "
            "ftxui/ header. Only files under src/tui/ or "
            "include/lazycmake/tui/ may depend on FTXUI "
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

# --- TUI library & executable (Phase 5+) -----------------------------------
# docs/DESIGN.md §14: the TUI layer implements the navigable, themeable,
# rebindable shell. All FTXUI-dependent code lives here.

if(LAZYCMAKE_BUILD_TUI)
    add_library(lazycmake_tui STATIC
        src/tui/application.cpp
        src/tui/color_helper.cpp
        src/tui/screen.cpp
        src/tui/startup_screen.cpp
        src/tui/wizard_screen.cpp
        src/tui/main_workspace.cpp
        src/tui/settings_screen.cpp
        src/tui/help_overlay.cpp
        src/tui/build_overlay.cpp
        src/tui/run_overlay.cpp
        src/tui/dependency_dialog.cpp
        src/tui/conflict_dialog.cpp
        src/tui/panels/file_panel.cpp
        src/tui/panels/target_panel.cpp
        src/tui/panels/output_panel.cpp
    )

    target_include_directories(lazycmake_tui
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    target_link_libraries(lazycmake_tui
        PUBLIC
            lazycmake_core
            ftxui::screen
            ftxui::dom
            ftxui::component
        PRIVATE
            fmt::fmt
            spdlog::spdlog
    )

    target_compile_features(lazycmake_tui PUBLIC cxx_std_20)

    if(MSVC)
        target_compile_options(lazycmake_tui PRIVATE /W4)
    else()
        target_compile_options(lazycmake_tui PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_executable(lazycmake
        src/tui/main.cpp
    )

    target_link_libraries(lazycmake
        PRIVATE
            lazycmake_tui
    )

    target_compile_features(lazycmake PUBLIC cxx_std_20)
endif()

# --- Install rules (Phase 8) -----------------------------------------------
# Install the core library, headers, TUI binary, and examples so that
# LazyCMake can be packaged for distribution (DEB, RPM, tarball, etc.).

include(GNUInstallDirs)

install(TARGETS lazycmake_core
    EXPORT LazyCMakeTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/lazycmake
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.hpp"
    PATTERN "tui" EXCLUDE
)

install(FILES cmake/LazyCMakeConfig.cmake
    DESTINATION ${CMAKE_INSTALL_DATADIR}/cmake/LazyCMake
)

if(LAZYCMAKE_BUILD_TUI)
    install(TARGETS lazycmake_tui lazycmake
        EXPORT LazyCMakeTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )

    install(DIRECTORY include/lazycmake/tui
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
    )
endif()

if(LAZYCMAKE_BUILD_EXAMPLES)
    install(TARGETS
        example01_basic_project_generation
        example02_event_bus_usage
        example03_config_system_demo
        example04_build_pipeline_demo
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}/examples
    )
endif()

install(DIRECTORY docs/
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
    FILES_MATCHING PATTERN "*.md"
)

install(DIRECTORY resources/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/lazycmake/resources
)

# --- CPack configuration ----------------------------------------------------
set(CPACK_PACKAGE_NAME "LazyCMake")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "lazygit, but for CMake")
set(CPACK_PACKAGE_VENDOR "LazyCMake Contributors")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/anomalyco/lazycmake")
set(CPACK_PACKAGE_MAINTAINER "LazyCMake Contributors")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_SOURCE_GENERATOR "TGZ")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
set(CPACK_RPM_PACKAGE_REQUIRES "glibc, libstdc++")

set(CPACK_COMPONENTS_ALL libraries headers binaries docs resources)

if(MSVC)
    set(CPACK_GENERATOR "NSIS;ZIP")
endif()

include(CPack)
