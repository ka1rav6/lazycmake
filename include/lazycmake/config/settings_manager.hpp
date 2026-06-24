#pragma once

// ==========================================================================
// SettingsManager — loads and merges application settings (§15.1)
//
// The SettingsManager implements the layered config resolution:
//   1. Compiled-in defaults (from AppSettings{} defaults)
//   2. System-wide config  (/etc/lazycmake/settings.json)
//   3. User config          (~/.config/lazycmake/settings.json)
//   4. Project-local        (<project>/.lazycmake/settings.json)
//   5. CLI overrides        (passed explicitly as a JSON string)
//
// Each layer partially overrides the previous one. Fields not mentioned
// in a layer keep their values from the previous layer. This is the
// "partial override" pattern used by Git config, ripgrep, etc.
//
// The manager can also detect file changes via timestamps for a simple
// form of hot-reload (§15.3). The full FileWatcher integration (using
// efsw) will be added in Phase 6.
// ==========================================================================

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "lazycmake/config/config_types.hpp"

namespace lazycmake::config {

class SettingsManager {
public:
    SettingsManager();

    // Load settings from all layers and merge them.
    // Call this once at startup, and again after a config file changes
    // (for hot-reload support).
    //
    // Returns the number of layers that were successfully loaded.
    // Errors are logged but don't abort — missing files are expected
    // (the user may not have a config directory yet).
    int loadAll();

    // Load a single config file (JSON) and merge it into the current
    // settings. Returns false if the file doesn't exist or is malformed.
    bool loadFile(const std::filesystem::path& path);

    // Load config from a raw JSON string (for CLI overrides).
    // Returns false if the string is not valid JSON.
    bool loadString(const std::string& jsonString);

    // Get the merged settings.
    [[nodiscard]] const AppSettings& settings() const;

    // Get a specific sub-settings.
    [[nodiscard]] const GeneralSettings& general() const;
    [[nodiscard]] const BuildSettings& build() const;
    [[nodiscard]] const EditorSettings& editor() const;

    // Set config search paths. Default:
    //   system:   /etc/lazycmake/
    //   user:     ~/.config/lazycmake/
    //   project:  <current working dir>/.lazycmake/
    void setSystemConfigDir(const std::filesystem::path& path);
    void setUserConfigDir(const std::filesystem::path& path);
    void setProjectConfigDir(const std::filesystem::path& path);

    // Check if a config file has changed since the last load (for
    // hot-reload). Returns true if the file's last write time differs
    // from the cached timestamp.
    [[nodiscard]] bool hasChanged() const;

    // Reset to compiled-in defaults (clears all overrides).
    void resetToDefaults();

private:
    // The merged (current) settings.
    AppSettings settings_;

    // Config directory paths.
    std::filesystem::path systemConfigDir_;
    std::filesystem::path userConfigDir_;
    std::filesystem::path projectConfigDir_;

    // Cached file timestamps for change detection.
    std::filesystem::file_time_type systemConfigTimestamp_;
    std::filesystem::file_time_type userConfigTimestamp_;
    std::filesystem::file_time_type projectConfigTimestamp_;

    // Load a JSON file from the given path and merge its contents into
    // the current settings. Returns true on success (even if file doesn't
    // exist — missing files are not errors).
    bool loadLayer(const std::filesystem::path& path);

    // Deep-merge a JSON object into the current settings.
    // This handles partial overrides: only the fields present in `layer`
    // override the corresponding fields in `settings_`.
    void mergeJson(const nlohmann::json& layer);
};

} // namespace lazycmake::config
