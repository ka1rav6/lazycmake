#pragma once

// ==========================================================================
// ThemeManager — loads and resolves color themes (§15)
//
// Themes are loaded from JSON files in the themes directories:
//   Built-in themes  → resources/themes/ (compiled into binary)
//   User themes      → ~/.config/lazycmake/themes/
//
// Each theme file defines a Theme struct (§15). The active theme is set
// by name (default: "dark"). The ThemeManager publishes a
// ConfigReloadedEvent when a theme file changes (for hot-reload).
//
// The TUI layer pulls ThemeColors from this manager rather than
// hardcoding colors, which is what makes theme switching work without
// per-component code changes (§14).
// ==========================================================================

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "lazycmake/config/config_types.hpp"

namespace lazycmake::config {

class ThemeManager {
public:
    ThemeManager();

    // Discover and load all themes from built-in and user directories.
    // Returns the number of themes loaded.
    int loadAll();

    // Load a single theme from a JSON file.
    bool loadFile(const std::filesystem::path& path);

    // Load a theme from a JSON string.
    bool loadString(const std::string& jsonString, const std::string& themeName = "");

    // Set the active theme by name. Returns false if the theme is
    // not found.
    bool setActiveTheme(const std::string& name);

    // Get the active theme. Always returns a valid theme (defaults
    // to built-in "dark" if no theme has been set).
    [[nodiscard]] const Theme& activeTheme() const;

    // Get the active theme's colors.
    [[nodiscard]] const ThemeColors& activeColors() const;

    // Get a theme by name. Returns nullptr if not found.
    [[nodiscard]] const Theme* findTheme(const std::string& name) const;

    // List all available theme names.
    [[nodiscard]] std::vector<std::string> availableThemes() const;

    // Check if any theme directory has new/changed files.
    [[nodiscard]] bool hasChanged() const;

    // Reset to built-in defaults.
    void resetToDefaults();

    // Set user themes directory.
    void setUserThemesDir(const std::filesystem::path& path);

private:
    std::map<std::string, Theme> themes_;
    std::string activeThemeName_ = "dark";

    std::filesystem::path userThemesDir_;

    // Register a theme, merging it into the map. If a theme with the
    // same name already exists, the new one overrides.
    void registerTheme(Theme theme);

    // Create the built-in default themes.
    void registerBuiltinThemes();
};

} // namespace lazycmake::config
