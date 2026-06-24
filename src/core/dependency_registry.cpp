#include "lazycmake/core/dependency_registry.hpp"

#include <fstream>

#include <spdlog/spdlog.h>

namespace lazycmake::core {

DependencyRegistry::DependencyRegistry() {
    loadBuiltin();
}

void DependencyRegistry::registerBuiltin(
    const std::string& name, const std::string& description,
    DependencySource source, const std::string& repo,
    const std::string& tag, const std::vector<std::string>& linkTargets,
    const std::vector<std::string>& tags) {
    presets_[name] = DependencyPreset{
        .name = name,
        .description = description,
        .source = source,
        .gitRepo = repo,
        .gitTag = tag,
        .linkTargets = linkTargets,
        .tags = tags,
    };
}

int DependencyRegistry::loadBuiltin() {
    registerBuiltin("fmt", "Modern C++ formatting library",
        DependencySource::FetchContent,
        "https://github.com/fmtlib/fmt.git", "11.0.2",
        {"fmt::fmt"}, {"formatting", "core"});

    registerBuiltin("spdlog", "Fast C++ logging library",
        DependencySource::FetchContent,
        "https://github.com/gabime/spdlog.git", "v1.14.1",
        {"spdlog::spdlog"}, {"logging", "core"});

    registerBuiltin("nlohmann_json", "JSON for Modern C++",
        DependencySource::FetchContent,
        "https://github.com/nlohmann/json.git", "v3.11.3",
        {"nlohmann_json::nlohmann_json"}, {"json", "serialization"});

    registerBuiltin("Catch2", "C++ test framework",
        DependencySource::FetchContent,
        "https://github.com/catchorg/Catch2.git", "v3.6.0",
        {"Catch2::Catch2", "Catch2::Catch2WithMain"}, {"testing"});

    registerBuiltin("Boost.Asio", "Networking library",
        DependencySource::FetchContent,
        "https://github.com/boostorg/asio.git", "boost-1.86.0",
        {"Boost::asio"}, {"networking"});

    registerBuiltin("SDL3", "Simple DirectMedia Layer 3",
        DependencySource::FetchContent,
        "https://github.com/libsdl-org/SDL.git", "main",
        {"SDL3::SDL3"}, {"graphics", "gaming"});

    registerBuiltin("GLFW", "Graphics library framework",
        DependencySource::FetchContent,
        "https://github.com/glfw/glfw.git", "3.4",
        {"glfw"}, {"graphics", "windowing"});

    registerBuiltin("ImGui", "Dear ImGui",
        DependencySource::FetchContent,
        "https://github.com/ocornut/imgui.git", "v1.91.0",
        {"imgui"}, {"graphics", "ui"});

    registerBuiltin("FTXUI", "Terminal UI library",
        DependencySource::FetchContent,
        "https://github.com/ArthurSonzogni/FTXUI.git", "v5.0.0",
        {"ftxui::screen", "ftxui::dom", "ftxui::component"}, {"tui"});

    registerBuiltin("EnTT", "Gaming entity system",
        DependencySource::FetchContent,
        "https://github.com/skypjack/entt.git", "v3.14.0",
        {"EnTT::EnTT"}, {"gaming", "ecs"});

    spdlog::info("DependencyRegistry: loaded {} built-in presets", presets_.size());
    return static_cast<int>(presets_.size());
}

int DependencyRegistry::loadUserPresets(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return 0;

    try {
        std::ifstream file(path);
        auto json = nlohmann::json::parse(file);

        int count = 0;
        for (const auto& [name, data] : json.items()) {
            DependencyPreset preset;
            preset.name = name;
            preset.description = data.value("description", "");
            preset.gitRepo = data.value("gitRepo", "");
            preset.gitTag = data.value("gitTag", "");
            preset.linkTargets = data.value("linkTargets", std::vector<std::string>{name + "::" + name});

            std::string src = data.value("source", "FetchContent");
            if (src == "CPM") preset.source = DependencySource::CPM;
            else if (src == "vcpkg") preset.source = DependencySource::VcpkgFindPackage;
            else preset.source = DependencySource::FetchContent;

            if (data.contains("tags"))
                preset.tags = data["tags"].get<std::vector<std::string>>();

            presets_[name] = preset;
            count++;
        }

        spdlog::info("DependencyRegistry: loaded {} user presets from {}", count, path.string());
        return count;
    } catch (const std::exception& e) {
        spdlog::warn("DependencyRegistry: failed to load user presets: {}", e.what());
        return 0;
    }
}

const DependencyPreset* DependencyRegistry::find(const std::string& name) const {
    auto it = presets_.find(name);
    if (it != presets_.end()) return &it->second;
    return nullptr;
}

std::vector<const DependencyPreset*> DependencyRegistry::findByName(
    const std::string& query) const {
    std::vector<const DependencyPreset*> results;
    auto lower = query;
    for (auto& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const auto& [name, preset] : presets_) {
        auto nameLower = name;
        for (auto& c : nameLower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (nameLower.find(lower) != std::string::npos) {
            results.push_back(&preset);
        }
    }
    return results;
}

std::vector<const DependencyPreset*> DependencyRegistry::findByTag(
    const std::string& tag) const {
    std::vector<const DependencyPreset*> results;
    for (const auto& [name, preset] : presets_) {
        for (const auto& t : preset.tags) {
            if (t == tag) {
                results.push_back(&preset);
                break;
            }
        }
    }
    return results;
}

std::vector<const DependencyPreset*> DependencyRegistry::all() const {
    std::vector<const DependencyPreset*> results;
    for (const auto& [name, preset] : presets_) {
        results.push_back(&preset);
    }
    return results;
}

DependencySpec DependencyRegistry::toSpec(const DependencyPreset& preset) const {
    DependencySpec spec;
    spec.name = preset.name;
    spec.source = preset.source;
    spec.gitRepo = preset.gitRepo;
    spec.gitTag = preset.gitTag;
    spec.linkTargets = preset.linkTargets;
    spec.enabled = false;
    return spec;
}

std::vector<DependencySpec> DependencyRegistry::toSpecs(
    const std::vector<std::string>& names) const {
    std::vector<DependencySpec> specs;
    for (const auto& name : names) {
        auto* preset = find(name);
        if (preset) specs.push_back(toSpec(*preset));
    }
    return specs;
}

} // namespace lazycmake::core
