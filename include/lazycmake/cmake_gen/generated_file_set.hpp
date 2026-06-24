#pragma once

// ==========================================================================
// GeneratedFileSet — the output type of every ICMakeGenerator.
//
// The design doc (§10.2) mandates that the generator produces an in-memory
// map of relative path → file content *before* touching disk. This lets the
// diff/hand-edit-detection logic (§10.4) run before overwriting anything,
// and keeps the generator itself a pure function (no I/O side effects).
//
// A GeneratedFileSet is essentially:  filesystem::path → std::string
// where the path is relative to the project root (e.g. "CMakeLists.txt",
// "engine/CMakeLists.txt", "cmake/dependencies.cmake").
// ==========================================================================

#include <filesystem>
#include <map>
#include <string>

namespace lazycmake::cmake_gen {

// Represents the complete set of files a CMake generator wants to write.
// The generator builds this in memory; callers (ProjectRepository or a
// CLI smoke-test) decide when/how to flush it to disk.
//
// Thread safety: not required — generators are called from a single thread.
class GeneratedFileSet {
public:
    // Add or replace a file. The path must be relative and use forward
    // slashes (generic format) for cross-platform portability.
    void addFile(std::filesystem::path relativePath, std::string content);

    // Returns true if this set contains a file at the given relative path.
    [[nodiscard]] bool contains(const std::filesystem::path& relativePath) const;

    // Retrieve the content of a file. Returns nullptr if not found.
    [[nodiscard]] const std::string* find(const std::filesystem::path& relativePath) const;

    // Number of files in the set.
    [[nodiscard]] std::size_t size() const;

    // Iterate over all (path, content) pairs. The caller gets a const
    // reference to the internal map, which is fine since iteration is
    // read-only and the set is used in a single-threaded context.
    [[nodiscard]] const std::map<std::filesystem::path, std::string>& files() const;

    // Remove all files from this set.
    void clear();

    // Check if the set is empty.
    [[nodiscard]] bool empty() const;

private:
    // Keys are relative paths (e.g. "CMakeLists.txt").
    // Values are the complete file content to write.
    std::map<std::filesystem::path, std::string> files_;
};

} // namespace lazycmake::cmake_gen
