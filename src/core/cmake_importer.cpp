#include "lazycmake/core/cmake_importer.hpp"

#include <fstream>
#include <regex>
#include <sstream>

#include <spdlog/spdlog.h>

namespace lazycmake::core {

bool CmakeImporter::hasExistingCMakeLists(const std::filesystem::path& dir) {
    return std::filesystem::exists(dir / "CMakeLists.txt");
}

ImportResult CmakeImporter::importFromDirectory(const std::filesystem::path& dir) {
    ImportResult result;
    Project& project = result.project;

    project.rootDir = std::filesystem::absolute(dir);
    project.name = project.rootDir.filename().string();
    project.language = Language::Cpp;
    project.cppStandard = CppStandard::Cpp20;

    auto cmakePath = dir / "CMakeLists.txt";
    if (!std::filesystem::exists(cmakePath)) {
        result.warnings.push_back("No CMakeLists.txt found; created empty project");
        return result;
    }

    std::ifstream file(cmakePath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto extractProjectName = [&]() -> std::string {
        std::regex projectRe(R"(project\s*\(\s*(\w+))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(content, match, projectRe)) {
            return match[1].str();
        }
        return dir.filename().string();
    };
    project.name = extractProjectName();

    auto extractLanguage = [&]() {
        if (content.find("LANGUAGES C") != std::string::npos ||
            content.find("LANGUAGES CXX") != std::string::npos) {
            project.language = Language::Cpp;
        } else if (content.find("LANGUAGES C") != std::string::npos) {
            project.language = Language::C;
        }
    };
    extractLanguage();

    auto extractStandard = [&]() {
        std::regex standardRe(R"(cxx_std_(\d+))", std::regex::icase);
        std::smatch match;
        if (std::regex_search(content, match, standardRe)) {
            int stdVal = std::stoi(match[1].str());
            switch (stdVal) {
                case 11: project.cppStandard = CppStandard::Cpp17; break;
                case 14: project.cppStandard = CppStandard::Cpp17; break;
                case 17: project.cppStandard = CppStandard::Cpp17; break;
                case 20: project.cppStandard = CppStandard::Cpp20; break;
                case 23: project.cppStandard = CppStandard::Cpp23; break;
            }
        }
    };
    extractStandard();

    auto extractTargets = [&]() {
        std::vector<std::regex> targetPatterns = {
            std::regex(R"(add_executable\s*\(\s*(\w+)\s+([^)]+))", std::regex::icase),
            std::regex(R"(add_library\s*\(\s*(\w+)\s+([^)]+))", std::regex::icase),
        };

        for (const auto& pattern : targetPatterns) {
            auto begin = content.cbegin();
            std::smatch match;
            while (std::regex_search(begin, content.cend(), match, pattern)) {
                std::string name = match[1].str();
                std::string sources = match[2].str();

                TargetKind kind = TargetKind::Executable;
                if (sources.find("STATIC") != std::string::npos) {
                    kind = TargetKind::StaticLibrary;
                } else if (sources.find("SHARED") != std::string::npos) {
                    kind = TargetKind::SharedLibrary;
                }

                Target target;
                target.name = name;
                target.kind = kind;

                std::istringstream srcStream(sources);
                std::string token;
                while (srcStream >> token) {
                    if (token == "STATIC" || token == "SHARED" || token == "EXCLUDE_FROM_ALL" ||
                        token == "OBJECT" || token == "MODULE") continue;
                    if (token.find(".cpp") != std::string::npos ||
                        token.find(".cxx") != std::string::npos ||
                        token.find(".cc") != std::string::npos ||
                        token.find(".c") != std::string::npos ||
                        token.find(".hpp") != std::string::npos ||
                        token.find(".h") != std::string::npos) {
                        target.sources.push_back({std::filesystem::path(token), false});
                    }
                }

                project.targets.push_back(target);
                result.targetsImported++;
                begin = match.suffix().first;
            }
        }
    };
    extractTargets();

    auto extractDependencies = [&]() {
        std::regex linkRe(R"(target_link_libraries\s*\(\s*(\w+)\s+(?:PUBLIC|PRIVATE|INTERFACE)\s+([^)]+))",
                          std::regex::icase);
        auto begin = content.cbegin();
        std::smatch match;
        while (std::regex_search(begin, content.cend(), match, linkRe)) {
            std::string targetName = match[1].str();
            std::string libs = match[2].str();

            std::istringstream libStream(libs);
            std::string lib;
            while (libStream >> lib) {
                if (lib == "PUBLIC" || lib == "PRIVATE" || lib == "INTERFACE") continue;
                bool isExternal = lib.find("::") != std::string::npos;
                if (isExternal) {
                    auto spec = DependencySpec{};
                    spec.name = lib.substr(0, lib.find("::"));
                    spec.linkTargets = {lib};
                    spec.enabled = true;
                    project.dependencies.push_back(spec);
                    result.depsImported++;
                }
                auto* target = project.findTarget(targetName);
                if (target) {
                    const_cast<Target*>(target)->linkTargets.push_back(lib);
                }
            }
            begin = match.suffix().first;
        }
    };
    extractDependencies();

    spdlog::info("CmakeImporter: imported '{}' with {} targets, {} deps",
                 project.name, result.targetsImported, result.depsImported);
    return result;
}

std::optional<ImportResult> CmakeImporter::tryImport(const std::filesystem::path& dir) {
    if (!hasExistingCMakeLists(dir)) {
        return std::nullopt;
    }
    try {
        return importFromDirectory(dir);
    } catch (const std::exception& e) {
        spdlog::warn("CmakeImporter: failed to import from {}: {}", dir.string(), e.what());
        return std::nullopt;
    }
}

} // namespace lazycmake::core
