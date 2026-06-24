#pragma once

// ==========================================================================
// ModernCMakeGenerator — the default CMake generator (§10)
//
// This is the main implementation of ICMakeGenerator. It "lowers" a Project
// into CMakeFile ASTs and uses CMakePrinter to render them to text.
//
// Key design decisions (matching §10.1):
//
//   - Always target-based: produces target_include_directories,
//     target_link_libraries, target_compile_features. Never produces
//     directory-scoped include_directories(), link_libraries(), or global
//     set(CMAKE_CXX_STANDARD ...). This is enforced structurally: the
//     CMakeFile AST (§10.2) has no node type that can represent such calls.
//
//   - One root CMakeLists.txt + one <target>/CMakeLists.txt per target
//     via add_subdirectory. This mirrors hand-written modern CMake and
//     keeps re-generation diffs small (changing one target doesn't rewrite
//     the whole tree's text).
//
//   - Multi-config awareness via $<CONFIG:Debug> generator expressions
//     for config-specific compile definitions, rather than duplicated
//     if(CMAKE_BUILD_TYPE STREQUAL "Debug") blocks.
//
//   - Dependencies are emitted to cmake/dependencies.cmake (included from
//     the root file), keeping the root CMakeLists.txt focused on project
//     structure rather than dependency wiring.
//
// File layout produced by this generator:
//
//   CMakeLists.txt                 — cmake_minimum_required, project(),
//                                     include(cmake/dependencies.cmake),
//                                     add_subdirectory() per target
//   cmake/dependencies.cmake       — FetchContent_Declare / find_package
//                                     blocks for each enabled dependency
//   <target_name>/CMakeLists.txt   — add_library/add_executable,
//                                     target_include_directories,
//                                     target_link_libraries,
//                                     target_compile_features,
//                                     target_compile_definitions
// ==========================================================================

#include <string>
#include <vector>

#include "lazycmake/cmake_gen/cmake_generator.hpp"
#include "lazycmake/cmake_gen/cmake_file.hpp"
#include "lazycmake/cmake_gen/cmake_printer.hpp"
#include "lazycmake/core/project.hpp"

namespace lazycmake::cmake_gen {

class ModernCMakeGenerator : public ICMakeGenerator {
public:
    ModernCMakeGenerator() = default;

    // Allow overriding printer options (e.g. for testing with specific
    // indentation or without the header comment).
    explicit ModernCMakeGenerator(CMakePrinter::Options printerOpts);

    // Main entry point: generate all CMake files for the given project.
    // Returns a GeneratedFileSet with relative paths and their contents.
    [[nodiscard]] GeneratedFileSet generate(const core::Project& project) const override;

private:
    CMakePrinter::Options printerOpts_;

    // --- AST builders (each builds a CMakeFile from part of Project) -----

    // Build the root CMakeLists.txt AST.
    // This contains: cmake_minimum_required, project(...),
    // include(cmake/dependencies.cmake), and add_subdirectory() for
    // every target that has its own CMakeLists.txt.
    [[nodiscard]] CMakeFile buildRootFile(const core::Project& project) const;

    // Build the cmake/dependencies.cmake AST.
    // Contains FetchContent_Declare/FetchContent_MakeAvailable for each
    // enabled DependencySpec with DependencySource::FetchContent,
    // and find_package() for VcpkgFindPackage/SystemFindPackage sources.
    [[nodiscard]] CMakeFile buildDependenciesFile(const core::Project& project) const;

    // Build a per-target CMakeLists.txt AST.
    // Contains: add_library/add_executable, target_include_directories,
    // target_link_libraries, target_compile_features,
    // target_compile_definitions.
    // Returns an empty CMakeFile if this target shouldn't have its own
    // file (e.g., it has no sources — though an InterfaceLibrary is a
    // valid target that gets its own file since it declares headers).
    [[nodiscard]] CMakeFile buildTargetFile(const core::Project& project,
                                              const core::Target& target) const;

    // --- Helpers for building arguments to AST statements ----------------

    // Build the LANGUAGES argument for project() based on the Project's
    // language setting.
    [[nodiscard]] std::string buildLanguageArg(const core::Project& project) const;

    // Resolve the effective C++ standard for a target, falling back to the
    // project-level default if the target doesn't override it.
    // Returns std::nullopt if neither the target nor the project specifies
    // a C++ standard (for a C-only project, this is expected).
    [[nodiscard]] std::optional<core::CppStandard> effectiveCppStandard(
        const core::Project& project, const core::Target& target) const;

    // Resolve the effective C standard similarly.
    [[nodiscard]] std::optional<core::CStandard> effectiveCStandard(
        const core::Project& project, const core::Target& target) const;

    // Determine the add_library/add_executable kind string and sources list
    // for a target. For InterfaceLibrary targets, sources are omitted
    // (they're header-only, sources are listed via target_sources only if
    // needed, but the header-only pattern uses interface directories).
    [[nodiscard]] std::string buildTargetKindString(core::TargetKind kind) const;
};

} // namespace lazycmake::cmake_gen
