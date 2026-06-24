#include "lazycmake/config/theme_manager.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace lazycmake::config {

ThemeManager::ThemeManager() {
    const char* home = std::getenv("HOME");
    if (home) {
        userThemesDir_ = std::filesystem::path(home) / ".config" / "lazycmake" / "themes";
    }

    // Always register built-in themes so the manager is never empty.
    registerBuiltinThemes();
}

int ThemeManager::loadAll() {
    int count = 0;

    // Load user themes (overrides built-in).
    if (!userThemesDir_.empty() && std::filesystem::exists(userThemesDir_)) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(userThemesDir_, ec)) {
            if (entry.path().extension() == ".json") {
                if (loadFile(entry.path())) {
                    ++count;
                }
            }
        }
    }

    return count;
}

bool ThemeManager::loadFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "ThemeManager: Could not open " << path.string() << std::endl;
        return false;
    }

    try {
        nlohmann::json j;
        file >> j;
        Theme theme = j.get<Theme>();

        // If the JSON doesn't have a name, derive it from the filename.
        if (theme.name.empty()) {
            theme.name = path.stem().string();
        }

        registerTheme(std::move(theme));
        return true;
    } catch (const std::exception& e) {
        std::cerr << "ThemeManager: Error loading " << path.string()
                  << ": " << e.what() << std::endl;
        return false;
    }
}

bool ThemeManager::loadString(const std::string& jsonString, const std::string& themeName) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonString);
        Theme theme = j.get<Theme>();
        if (theme.name.empty() && !themeName.empty()) {
            theme.name = themeName;
        }
        registerTheme(std::move(theme));
        return true;
    } catch (const std::exception& e) {
        std::cerr << "ThemeManager: Failed to load theme: " << e.what() << std::endl;
        return false;
    }
}

bool ThemeManager::setActiveTheme(const std::string& name) {
    if (themes_.find(name) != themes_.end()) {
        activeThemeName_ = name;
        return true;
    }
    std::cerr << "ThemeManager: Theme '" << name << "' not found" << std::endl;
    return false;
}

const Theme& ThemeManager::activeTheme() const {
    auto it = themes_.find(activeThemeName_);
    if (it != themes_.end()) {
        return it->second;
    }
    // Fallback: return the first available theme.
    // There's always at least the built-in "dark" theme.
    static const Theme fallback;
    if (themes_.empty()) {
        return fallback;
    }
    return themes_.begin()->second;
}

const ThemeColors& ThemeManager::activeColors() const {
    return activeTheme().colors;
}

const Theme* ThemeManager::findTheme(const std::string& name) const {
    auto it = themes_.find(name);
    if (it != themes_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> ThemeManager::availableThemes() const {
    std::vector<std::string> names;
    names.reserve(themes_.size());
    for (const auto& [name, _] : themes_) {
        names.push_back(name);
    }
    return names;
}

bool ThemeManager::hasChanged() const {
    // Would need FileWatcher integration (Phase 6) for proper change
    // detection. Placeholder always returns false for now.
    return false;
}

void ThemeManager::resetToDefaults() {
    themes_.clear();
    registerBuiltinThemes();
    activeThemeName_ = "dark";
}

void ThemeManager::setUserThemesDir(const std::filesystem::path& path) {
    userThemesDir_ = path;
}

void ThemeManager::registerTheme(Theme theme) {
    // User themes override built-in themes with the same name.
    // This is the layered resolution pattern (§15.1): later layers
    // override earlier ones.
    themes_[theme.name] = std::move(theme);
}

void ThemeManager::registerBuiltinThemes() {
    // Dark theme (Tokyo Night inspired) — the default.
    Theme dark;
    dark.name = "dark";
    dark.description = "Dark theme (Tokyo Night inspired)";
    dark.colors = ThemeColors{};
    registerTheme(std::move(dark));

    // Light theme.
    Theme light;
    light.name = "light";
    light.description = "Light theme";
    light.colors = {
        .background = "#f5f5f5",
        .foreground = "#2c3e50",
        .accent = "#3498db",
        .accentSecondary = "#9b59b6",
        .success = "#27ae60",
        .warning = "#f39c12",
        .error = "#e74c3c",
        .info = "#1abc9c",
        .panelBorderFocused = "#3498db",
        .panelBorderUnfocused = "#bdc3c7",
        .panelTitle = "#2c3e50",
        .panelBackground = "#ffffff",
        .listSelectionBackground = "#d5e8f7",
        .listSelectionForeground = "#2c3e50",
        .listInactiveBackground = "#ffffff",
        .listInactiveForeground = "#95a5a6",
    };
    registerTheme(std::move(light));

    // Nord theme.
    Theme nord;
    nord.name = "nord";
    nord.description = "Nord polar night theme";
    nord.colors = {
        .background = "#2e3440",
        .foreground = "#d8dee9",
        .accent = "#88c0d0",
        .accentSecondary = "#b48ead",
        .success = "#a3be8c",
        .warning = "#ebcb8b",
        .error = "#bf616a",
        .info = "#81a1c1",
        .panelBorderFocused = "#88c0d0",
        .panelBorderUnfocused = "#4c566a",
        .panelTitle = "#d8dee9",
        .panelBackground = "#2e3440",
        .listSelectionBackground = "#3b4252",
        .listSelectionForeground = "#eceff4",
        .listInactiveBackground = "#2e3440",
        .listInactiveForeground = "#4c566a",
    };
    registerTheme(std::move(nord));
}

} // namespace lazycmake::config
