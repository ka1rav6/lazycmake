#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "lazycmake/core/project.hpp"

namespace lazycmake::core {

struct ImportResult {
    Project project;
    std::vector<std::string> warnings;
    int targetsImported = 0;
    int depsImported = 0;
};

class CmakeImporter {
public:
    static ImportResult importFromDirectory(const std::filesystem::path& dir);
    static bool hasExistingCMakeLists(const std::filesystem::path& dir);
    static std::optional<ImportResult> tryImport(const std::filesystem::path& dir);
};

} // namespace lazycmake::core
