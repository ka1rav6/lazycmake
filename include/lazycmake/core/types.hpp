#pragma once

#include <cstdint>
#include <string>

namespace lazycmake::core {

enum class Language {
    C,
    Cpp,
    Both
};

enum class CppStandard {
    Cpp17,
    Cpp20,
    Cpp23
};

enum class CStandard {
    C11,
    C17
};

enum class TargetKind {
    Executable,
    StaticLibrary,
    SharedLibrary,
    InterfaceLibrary
};

enum class BuildType {
    Debug,
    Release,
    RelWithDebInfo,
    MinSizeRel
};

enum class GeneratorKind {
    Ninja,
    UnixMakefiles,
    NMake
};

enum class DependencySource {
    FetchContent,
    CPM,
    VcpkgFindPackage,
    SystemFindPackage
};

// --- String conversion helpers -------------------------------------------
// Centralized here so the JSON (de)serializers and the CMake generator
// share a single source of truth for the on-disk / generated-text spelling
// of each enum value. Keeping these as free functions (not enum methods)
// keeps the enums themselves trivial value types.

[[nodiscard]] std::string toString(Language value);
[[nodiscard]] std::string toString(CppStandard value);
[[nodiscard]] std::string toString(CStandard value);
[[nodiscard]] std::string toString(TargetKind value);
[[nodiscard]] std::string toString(BuildType value);
[[nodiscard]] std::string toString(GeneratorKind value);
[[nodiscard]] std::string toString(DependencySource value);

[[nodiscard]] Language languageFromString(const std::string& value);
[[nodiscard]] CppStandard cppStandardFromString(const std::string& value);
[[nodiscard]] CStandard cStandardFromString(const std::string& value);
[[nodiscard]] TargetKind targetKindFromString(const std::string& value);
[[nodiscard]] BuildType buildTypeFromString(const std::string& value);
[[nodiscard]] GeneratorKind generatorKindFromString(const std::string& value);
[[nodiscard]] DependencySource dependencySourceFromString(const std::string& value);

// The numeric CMake feature name e.g. CppStandard::Cpp20 -> "cxx_std_20"
[[nodiscard]] std::string toCMakeFeatureString(CppStandard value);
[[nodiscard]] std::string toCMakeFeatureString(CStandard value);

}  // namespace lazycmake::core
