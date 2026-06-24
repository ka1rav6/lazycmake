#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lazycmake/core/types.hpp"

namespace lazycmake::core {

// A single dependency the project can depend on. DependencyRegistry (not
// modeled in this header) holds the catalog of *available* specs merged
// from built-in/user/plugin layers; a Project only stores the specs it has
// actually enabled, copied by value so a project's dependency list is
// self-contained and doesn't break if the registry's catalog later changes.
struct DependencySpec {
    std::string name;                        // e.g. "fmt", "SDL3"
    DependencySource source = DependencySource::FetchContent;
    std::string gitRepo;                      // used by FetchContent / CPM
    std::string gitTag;
    std::vector<std::string> linkTargets;     // e.g. {"fmt::fmt"}
    bool enabled = false;

    bool operator==(const DependencySpec& other) const = default;
};

// Reference to a source file tracked by a Target. `autoDetected` records
// whether FileWatcher added it (vs. the user explicitly adding it through
// the Files panel) purely for UI/UX purposes (e.g. a different icon); it
// has no effect on the generated CMake.
struct SourceFileRef {
    std::filesystem::path path;     // relative to project root
    bool autoDetected = false;

    bool operator==(const SourceFileRef& other) const = default;
};

struct Target {
    std::string name;
    TargetKind kind = TargetKind::Executable;
    std::vector<SourceFileRef> sources;
    std::vector<std::filesystem::path> includeDirs;
    std::vector<std::string> linkTargets;   // names of other Targets or DependencySpec link names
    std::optional<CppStandard> cppStandard; // overrides Project-level default if set
    std::map<std::string, std::string> compileDefinitions;

    bool operator==(const Target& other) const = default;
};

struct BuildConfig {
    GeneratorKind generator = GeneratorKind::Ninja;
    BuildType defaultBuildType = BuildType::Debug;
    std::string compilerOverride;           // empty = let CMake auto-detect
    std::filesystem::path buildDir = "build";

    bool operator==(const BuildConfig& other) const = default;
};

// The single source of truth for a LazyCMake project. The CMake generator
// (lazycmake::cmake_gen) is a pure function of this struct; the TUI never
// edits generated CMakeLists.txt text directly, only this model, then
// re-invokes the generator. See docs/DESIGN.md §6 and §10.
struct Project {
    std::string name;
    Language language = Language::Cpp;
    std::optional<CppStandard> cppStandard;
    std::optional<CStandard> cStandard;
    std::filesystem::path rootDir;
    std::vector<Target> targets;
    std::vector<DependencySpec> dependencies;
    BuildConfig buildConfig;
    std::string schemaVersion = "1.0";

    bool operator==(const Project& other) const = default;

    // Convenience lookups used throughout Core and the TUI layer.
    [[nodiscard]] const Target* findTarget(const std::string& targetName) const;
    [[nodiscard]] Target* findTarget(const std::string& targetName);
    [[nodiscard]] bool hasTargetNamed(const std::string& targetName) const;
};

// --- nlohmann::json (de)serialization hooks ------------------------------
// Declared via the ADL to_json/from_json pattern so std::filesystem::path,
// std::optional, and the enums above all round-trip through
// ProjectRepository's load/save without any manual JSON-building code
// scattered elsewhere.

void to_json(nlohmann::json& j, const DependencySpec& spec);
void from_json(const nlohmann::json& j, DependencySpec& spec);

void to_json(nlohmann::json& j, const SourceFileRef& ref);
void from_json(const nlohmann::json& j, SourceFileRef& ref);

void to_json(nlohmann::json& j, const Target& target);
void from_json(const nlohmann::json& j, Target& target);

void to_json(nlohmann::json& j, const BuildConfig& config);
void from_json(const nlohmann::json& j, BuildConfig& config);

void to_json(nlohmann::json& j, const Project& project);
void from_json(const nlohmann::json& j, Project& project);

}  // namespace lazycmake::core
