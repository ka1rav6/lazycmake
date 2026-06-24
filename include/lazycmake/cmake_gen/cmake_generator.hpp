#pragma once

// ==========================================================================
// ICMakeGenerator — strategy-pattern interface for CMake generation (§10.2)
//
// The design doc calls for a strategy pattern seam so alternate generators
// can be plugged in without touching existing code (Open/Closed Principle).
// For Phase 2 there is one concrete implementation: ModernCMakeGenerator.
// Future generators might produce:
//   - A flat (non-target-based) legacy CMake for compatibility.
//   - A Meson build file from the same Project model.
//   - A Bazel BUILD file export.
// All would implement this interface.
//
// The single method `generate()` takes a Project (from Core layer) and
// produces a GeneratedFileSet: a map of relative paths → file contents.
// The generator never writes to disk or reads files — it is a pure
// function from Project → map<path, string>. This is the property that
// makes it testable with golden-file tests (§18) and makes the hand-edit
// detection (§10.4) tractable.
// ==========================================================================

#include "lazycmake/core/project.hpp"
#include "lazycmake/cmake_gen/generated_file_set.hpp"

namespace lazycmake::cmake_gen {

class ICMakeGenerator {
public:
    virtual ~ICMakeGenerator() = default;

    // Generate the complete set of CMake files for the given project.
    // The returned GeneratedFileSet contains relative paths like:
    //   "CMakeLists.txt"          — root file
    //   "<target>/CMakeLists.txt" — one per target with sources
    //   "cmake/dependencies.cmake" — FetchContent/CPM/vcpkg blocks
    //
    // The caller (ProjectRepository or similar) decides when to write
    // these to disk, enabling hand-edit detection before overwrite.
    [[nodiscard]] virtual GeneratedFileSet generate(const core::Project& project) const = 0;
};

} // namespace lazycmake::cmake_gen
