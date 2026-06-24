#include "lazycmake/config/settings_manager.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

namespace lazycmake::config {

SettingsManager::SettingsManager() {
    // Set default config directory paths.
    systemConfigDir_ = std::filesystem::path("/etc/lazycmake");

    // XDG_CONFIG_HOME takes precedence, fall back to ~/.config.
    const char* xdgConfigHome = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfigHome && strlen(xdgConfigHome) > 0) {
        userConfigDir_ = std::filesystem::path(xdgConfigHome) / "lazycmake";
    } else {
        const char* home = std::getenv("HOME");
        if (home) {
            userConfigDir_ = std::filesystem::path(home) / ".config" / "lazycmake";
        }
    }

    // Default project config is in the current directory's .lazycmake.
    projectConfigDir_ = std::filesystem::current_path() / ".lazycmake";
}

int SettingsManager::loadAll() {
    int layersLoaded = 0;

    // Start fresh from defaults.
    resetToDefaults();

    // Layer 1: System-wide config.
    if (loadLayer(systemConfigDir_ / "settings.json")) {
        ++layersLoaded;
    }

    // Layer 2: User config.
    if (loadLayer(userConfigDir_ / "settings.json")) {
        ++layersLoaded;
    }

    // Layer 3: Project-local config.
    if (loadLayer(projectConfigDir_ / "settings.json")) {
        ++layersLoaded;
    }

    return layersLoaded;
}

bool SettingsManager::loadFile(const std::filesystem::path& path) {
    return loadLayer(path);
}

bool SettingsManager::loadString(const std::string& jsonString) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonString);
        mergeJson(j);
        return true;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "SettingsManager: Failed to parse config string: " << e.what() << std::endl;
        return false;
    }
}

const AppSettings& SettingsManager::settings() const {
    return settings_;
}

const GeneralSettings& SettingsManager::general() const {
    return settings_.general;
}

const BuildSettings& SettingsManager::build() const {
    return settings_.build;
}

const EditorSettings& SettingsManager::editor() const {
    return settings_.editor;
}

void SettingsManager::setSystemConfigDir(const std::filesystem::path& path) {
    systemConfigDir_ = path;
}

void SettingsManager::setUserConfigDir(const std::filesystem::path& path) {
    userConfigDir_ = path;
}

void SettingsManager::setProjectConfigDir(const std::filesystem::path& path) {
    projectConfigDir_ = path;
}

bool SettingsManager::hasChanged() const {
    auto checkTimestamp = [](const std::filesystem::path& path,
                              std::filesystem::file_time_type cached) -> bool {
        std::error_code ec;
        auto current = std::filesystem::last_write_time(path, ec);
        if (ec) return false;  // file doesn't exist or can't be read
        return current != cached;
    };

    return checkTimestamp(systemConfigDir_ / "settings.json", systemConfigTimestamp_)
        || checkTimestamp(userConfigDir_ / "settings.json", userConfigTimestamp_)
        || checkTimestamp(projectConfigDir_ / "settings.json", projectConfigTimestamp_);
}

void SettingsManager::resetToDefaults() {
    settings_ = AppSettings{};
}

bool SettingsManager::loadLayer(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return false;  // Missing files are not errors.
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "SettingsManager: Could not open " << path.string() << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;
        mergeJson(j);

        // Cache the timestamp for change detection.
        auto& ts = (path.parent_path() == systemConfigDir_)
            ? systemConfigTimestamp_
            : (path.parent_path() == userConfigDir_)
                ? userConfigTimestamp_
                : projectConfigTimestamp_;
        ts = std::filesystem::last_write_time(path, ec);

        return true;
    } catch (const nlohmann::json::parse_error& e) {
        std::cerr << "SettingsManager: Invalid JSON in " << path.string()
                  << ": " << e.what() << std::endl;
        return false;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "SettingsManager: Error reading " << path.string()
                  << ": " << e.what() << std::endl;
        return false;
    }
}

void SettingsManager::mergeJson(const nlohmann::json& layer) {
    // Deep merge: iterate over top-level keys in the layer and if the
    // corresponding value in settings_ is also an object, recurse.
    // This implements the "partial override" pattern from §15.1.
    //
    // We use the existing JSON serialization by round-tripping through
    // JSON. The approach:
    //   1. Serialize current settings_ to JSON.
    //   2. Deep-merge the layer into that JSON.
    //   3. Deserialize back to AppSettings.
    //
    // This is simple and correct, though not maximally efficient.
    // Performance doesn't matter since config is loaded once at startup
    // and on hot-reload (infrequent).

    nlohmann::json current = settings_;

    // Recursive deep merge.
    // For each key in `layer`:
    //   - If both values are objects, recurse.
    //   - Otherwise, override.
    std::function<void(nlohmann::json&, const nlohmann::json&)> deepMerge =
        [&](nlohmann::json& target, const nlohmann::json& source) {
            for (auto it = source.begin(); it != source.end(); ++it) {
                if (it->is_object() && target.contains(it.key()) &&
                    target[it.key()].is_object()) {
                    // Both are objects — recurse.
                    deepMerge(target[it.key()], *it);
                } else {
                    // Leaf value or type mismatch — override.
                    target[it.key()] = *it;
                }
            }
        };

    deepMerge(current, layer);

    // Deserialize back to AppSettings.
    try {
        settings_ = current.get<AppSettings>();
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "SettingsManager: Merge result is invalid: " << e.what() << std::endl;
    }
}

} // namespace lazycmake::config
