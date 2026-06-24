#include "lazycmake/core/project.hpp"

#include <algorithm>

namespace lazycmake::core {

const Target* Project::findTarget(const std::string& targetName) const {
    auto it = std::find_if(targets.begin(), targets.end(), [&](const Target& t) {
        return t.name == targetName;
    });
    return it == targets.end() ? nullptr : &(*it);
}

Target* Project::findTarget(const std::string& targetName) {
    auto it = std::find_if(targets.begin(), targets.end(), [&](const Target& t) {
        return t.name == targetName;
    });
    return it == targets.end() ? nullptr : &(*it);
}

bool Project::hasTargetNamed(const std::string& targetName) const {
    return findTarget(targetName) != nullptr;
}

// --- DependencySpec -------------------------------------------------------

void to_json(nlohmann::json& j, const DependencySpec& spec) {
    j = nlohmann::json{
        {"name", spec.name},
        {"source", toString(spec.source)},
        {"gitRepo", spec.gitRepo},
        {"gitTag", spec.gitTag},
        {"linkTargets", spec.linkTargets},
        {"enabled", spec.enabled},
    };
}

void from_json(const nlohmann::json& j, DependencySpec& spec) {
    j.at("name").get_to(spec.name);
    spec.source = dependencySourceFromString(j.at("source").get<std::string>());
    j.at("gitRepo").get_to(spec.gitRepo);
    j.at("gitTag").get_to(spec.gitTag);
    j.at("linkTargets").get_to(spec.linkTargets);
    j.at("enabled").get_to(spec.enabled);
}

// --- SourceFileRef ---------------------------------------------------------

void to_json(nlohmann::json& j, const SourceFileRef& ref) {
    j = nlohmann::json{
        {"path", ref.path.generic_string()},  // generic_string for cross-platform JSON portability
        {"autoDetected", ref.autoDetected},
    };
}

void from_json(const nlohmann::json& j, SourceFileRef& ref) {
    ref.path = std::filesystem::path(j.at("path").get<std::string>());
    j.at("autoDetected").get_to(ref.autoDetected);
}

// --- Target ----------------------------------------------------------------

void to_json(nlohmann::json& j, const Target& target) {
    std::vector<std::string> includeDirStrings;
    includeDirStrings.reserve(target.includeDirs.size());
    for (const auto& dir : target.includeDirs) {
        includeDirStrings.push_back(dir.generic_string());
    }

    j = nlohmann::json{
        {"name", target.name},
        {"kind", toString(target.kind)},
        {"sources", target.sources},
        {"includeDirs", includeDirStrings},
        {"linkTargets", target.linkTargets},
        {"compileDefinitions", target.compileDefinitions},
    };

    if (target.cppStandard.has_value()) {
        j["cppStandard"] = toString(*target.cppStandard);
    } else {
        j["cppStandard"] = nullptr;
    }
}

void from_json(const nlohmann::json& j, Target& target) {
    j.at("name").get_to(target.name);
    target.kind = targetKindFromString(j.at("kind").get<std::string>());
    j.at("sources").get_to(target.sources);

    target.includeDirs.clear();
    for (const auto& dirStr : j.at("includeDirs")) {
        target.includeDirs.emplace_back(dirStr.get<std::string>());
    }

    j.at("linkTargets").get_to(target.linkTargets);
    j.at("compileDefinitions").get_to(target.compileDefinitions);

    if (j.contains("cppStandard") && !j.at("cppStandard").is_null()) {
        target.cppStandard = cppStandardFromString(j.at("cppStandard").get<std::string>());
    } else {
        target.cppStandard = std::nullopt;
    }
}

// --- BuildConfig -------------------------------------------------------------

void to_json(nlohmann::json& j, const BuildConfig& config) {
    j = nlohmann::json{
        {"generator", toString(config.generator)},
        {"defaultBuildType", toString(config.defaultBuildType)},
        {"compilerOverride", config.compilerOverride},
        {"buildDir", config.buildDir.generic_string()},
    };
}

void from_json(const nlohmann::json& j, BuildConfig& config) {
    config.generator = generatorKindFromString(j.at("generator").get<std::string>());
    config.defaultBuildType = buildTypeFromString(j.at("defaultBuildType").get<std::string>());
    j.at("compilerOverride").get_to(config.compilerOverride);
    config.buildDir = std::filesystem::path(j.at("buildDir").get<std::string>());
}

// --- Project -----------------------------------------------------------------

void to_json(nlohmann::json& j, const Project& project) {
    j = nlohmann::json{
        {"name", project.name},
        {"language", toString(project.language)},
        {"rootDir", project.rootDir.generic_string()},
        {"targets", project.targets},
        {"dependencies", project.dependencies},
        {"buildConfig", project.buildConfig},
        {"schemaVersion", project.schemaVersion},
    };

    if (project.cppStandard.has_value()) {
        j["cppStandard"] = toString(*project.cppStandard);
    } else {
        j["cppStandard"] = nullptr;
    }

    if (project.cStandard.has_value()) {
        j["cStandard"] = toString(*project.cStandard);
    } else {
        j["cStandard"] = nullptr;
    }
}

void from_json(const nlohmann::json& j, Project& project) {
    j.at("name").get_to(project.name);
    project.language = languageFromString(j.at("language").get<std::string>());
    project.rootDir = std::filesystem::path(j.at("rootDir").get<std::string>());
    j.at("targets").get_to(project.targets);
    j.at("dependencies").get_to(project.dependencies);
    j.at("buildConfig").get_to(project.buildConfig);
    j.at("schemaVersion").get_to(project.schemaVersion);

    if (j.contains("cppStandard") && !j.at("cppStandard").is_null()) {
        project.cppStandard = cppStandardFromString(j.at("cppStandard").get<std::string>());
    } else {
        project.cppStandard = std::nullopt;
    }

    if (j.contains("cStandard") && !j.at("cStandard").is_null()) {
        project.cStandard = cStandardFromString(j.at("cStandard").get<std::string>());
    } else {
        project.cStandard = std::nullopt;
    }
}

}  // namespace lazycmake::core
