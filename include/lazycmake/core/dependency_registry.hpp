#pragma once

#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lazycmake/core/project.hpp"

namespace lazycmake::core {

struct DependencyPreset {
    std::string name;
    std::string description;
    DependencySource source = DependencySource::FetchContent;
    std::string gitRepo;
    std::string gitTag;
    std::vector<std::string> linkTargets;
    std::vector<std::string> tags;
};

class DependencyRegistry {
public:
    DependencyRegistry();

    int loadBuiltin();
    int loadUserPresets(const std::filesystem::path& path);

    const DependencyPreset* find(const std::string& name) const;

    std::vector<const DependencyPreset*> findByName(const std::string& query) const;
    std::vector<const DependencyPreset*> findByTag(const std::string& tag) const;
    std::vector<const DependencyPreset*> all() const;

    DependencySpec toSpec(const DependencyPreset& preset) const;
    std::vector<DependencySpec> toSpecs(const std::vector<std::string>& names) const;

private:
    std::map<std::string, DependencyPreset> presets_;
    void registerBuiltin(const std::string& name, const std::string& description,
                         DependencySource source, const std::string& repo,
                         const std::string& tag, const std::vector<std::string>& linkTargets,
                         const std::vector<std::string>& tags = {});
};

} // namespace lazycmake::core
