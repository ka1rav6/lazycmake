// ==========================================================================
// ThemeManager tests
//
// Tests cover:
//   - Built-in themes are registered
//   - Theme lookup by name
//   - Active theme management
//   - Loading themes from JSON string
//   - Fallback behavior
// ==========================================================================

#include <catch2/catch_test_macros.hpp>

#include "lazycmake/config/theme_manager.hpp"

using namespace lazycmake::config;

TEST_CASE("Built-in dark theme is available and active by default", "[config][theme]") {
    ThemeManager mgr;

    CHECK(mgr.activeTheme().name == "dark");
    CHECK(mgr.activeTheme().description.find("Dark") != std::string::npos);

    // Dark theme specific colors.
    const auto& colors = mgr.activeColors();
    CHECK(colors.background == "#1a1b26");
    CHECK(colors.foreground == "#a9b1d6");
    CHECK(colors.accent == "#7aa2f7");
    CHECK(colors.error == "#f7768e");
    CHECK(colors.success == "#9ece6a");
}

TEST_CASE("Multiple built-in themes are available", "[config][theme]") {
    ThemeManager mgr;

    auto themes = mgr.availableThemes();

    // Should have at least 3 built-in themes: dark, light, nord.
    CHECK(themes.size() >= 3);

    bool hasDark = false, hasLight = false, hasNord = false;
    for (const auto& name : themes) {
        if (name == "dark") hasDark = true;
        if (name == "light") hasLight = true;
        if (name == "nord") hasNord = true;
    }
    CHECK(hasDark);
    CHECK(hasLight);
    CHECK(hasNord);
}

TEST_CASE("Switching active theme works", "[config][theme]") {
    ThemeManager mgr;

    // Switch to light theme.
    bool switched = mgr.setActiveTheme("light");
    CHECK(switched);
    CHECK(mgr.activeTheme().name == "light");

    // Light theme should have light-colored background.
    CHECK(mgr.activeColors().background == "#f5f5f5");
    CHECK(mgr.activeColors().foreground == "#2c3e50");

    // Switch to nord.
    switched = mgr.setActiveTheme("nord");
    CHECK(switched);
    CHECK(mgr.activeTheme().name == "nord");
    CHECK(mgr.activeColors().background == "#2e3440");
}

TEST_CASE("Switching to nonexistent theme returns false, keeps current", "[config][theme]") {
    ThemeManager mgr;

    // Switch to light first.
    mgr.setActiveTheme("light");
    CHECK(mgr.activeTheme().name == "light");

    // Try switching to nonexistent.
    bool switched = mgr.setActiveTheme("totally_fake_theme");
    CHECK_FALSE(switched);

    // The active theme should remain "light".
    CHECK(mgr.activeTheme().name == "light");
}

TEST_CASE("Find theme by name returns correct theme", "[config][theme]") {
    ThemeManager mgr;

    const Theme* nord = mgr.findTheme("nord");
    REQUIRE(nord != nullptr);
    CHECK(nord->name == "nord");
    CHECK(nord->colors.background == "#2e3440");

    // Non-existent theme returns nullptr.
    CHECK(mgr.findTheme("nonexistent") == nullptr);
}

TEST_CASE("Loading a theme from JSON string works", "[config][theme]") {
    ThemeManager mgr;

    bool loaded = mgr.loadString(R"({
        "name": "custom_theme",
        "description": "A custom test theme",
        "colors": {
            "background": "#000000",
            "foreground": "#ffffff",
            "accent": "#ff0000"
        }
    })");

    CHECK(loaded);

    // Should be able to find the new theme.
    const Theme* custom = mgr.findTheme("custom_theme");
    REQUIRE(custom != nullptr);
    CHECK(custom->description == "A custom test theme");
    CHECK(custom->colors.background == "#000000");
    CHECK(custom->colors.foreground == "#ffffff");

    // Non-overridden colors should have their default values
    // (from the default ThemeColors constructor).
    CHECK(custom->colors.error == "#f7768e");  // default
}

TEST_CASE("Reset to defaults restores built-in themes", "[config][theme]") {
    ThemeManager mgr;

    // Add a custom theme and switch to it.
    mgr.loadString(R"({"name": "custom_dark", "description": "My custom theme"})");
    mgr.setActiveTheme("custom_dark");
    CHECK(mgr.activeTheme().name == "custom_dark");

    // Reset.
    mgr.resetToDefaults();

    // After reset, active theme should be back to "dark".
    CHECK(mgr.activeTheme().name == "dark");

    // Custom theme should be gone.
    CHECK(mgr.findTheme("custom_dark") == nullptr);
}

// End of theme manager tests
