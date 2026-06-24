// ==========================================================================
// Example 3: Configuration System Demo
//
// This example demonstrates the layered configuration system (§15):
//   1. SettingsManager — application settings with layered resolution
//   2. KeymapManager — keybinding resolution with context
//   3. ThemeManager — color themes with active theme switching
//   4. TemplateManager — project templates with {{placeholder}} substitution
//
// The config system implements the "partial override" pattern:
// each layer (built-in defaults → system → user → project → CLI)
// only needs to list the fields it wants to change. Everything else
// falls through to the previous layer.
//
// To build: (similar to example 1, include src/config/*.cpp)
// ==========================================================================

#include <iostream>
#include <sstream>

#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/config/template_manager.hpp"

using namespace lazycmake::config;

int main() {
    std::cout << "=== LazyCMake Example 3: Configuration System ===\n\n";

    // ======================================================================
    // Part 1: SettingsManager — application settings
    //
    // The SettingsManager loads settings from layered JSON files and
    // deep-merges them. If no files exist (e.g., first run), all settings
    // use sensible compiled-in defaults.
    //
    // Each config file is a partial override — only listing what you
    // want to change. For example, a user's settings.json might contain:
    //   { "build": { "parallelJobs": 8 } }
    // All other settings (general, editor) keep their defaults.
    // ======================================================================

    std::cout << "--- SettingsManager ---\n";

    SettingsManager settings;

    // Default settings (no files loaded).
    std::cout << "  Default parallelJobs: " << settings.build().parallelJobs << "\n";
    std::cout << "  Default autoConfigure: " << std::boolalpha
              << settings.build().autoConfigure << "\n";
    std::cout << "  Default language: " << settings.general().language << "\n";

    // Load a CLI override as a JSON string.
    // In practice, this comes from --flags on the command line.
    settings.loadString(R"({
        "build": {
            "parallelJobs": 4,
            "verboseBuild": true
        },
        "general": {
            "language": "de"
        }
    })");

    std::cout << "  After CLI override:\n";
    std::cout << "    parallelJobs: " << settings.build().parallelJobs << "\n";
    std::cout << "    verboseBuild: " << settings.build().verboseBuild << "\n";
    std::cout << "    language: " << settings.general().language << "\n";
    std::cout << "    autoConfigure (unchanged): " << settings.build().autoConfigure << "\n";

    // Load another layer on top (later layers win).
    settings.loadString(R"({"build": {"parallelJobs": 8}})");
    std::cout << "  After second override, parallelJobs: "
              << settings.build().parallelJobs << "\n";

    // Reset to defaults.
    settings.resetToDefaults();
    std::cout << "  After reset, parallelJobs: " << settings.build().parallelJobs << "\n\n";

    // ======================================================================
    // Part 2: KeymapManager — keybinding resolution
    //
    // The KeymapManager maps raw key events (e.g., "b", "ctrl+q") to
    // semantic actions (e.g., "build", "quit"). It uses contexts
    // ("global", "main_workspace", "wizard", etc.) so the same key can
    // mean different things in different panels.
    //
    // The default keymap is vim-like and compiled into the binary.
    // Users override only what they want to change.
    // ======================================================================

    std::cout << "--- KeymapManager ---\n";

    KeymapManager keymap;

    // Resolve keys in different contexts.
    std::cout << "  'q' in global: " << keymap.resolve("global", "q") << "\n";
    std::cout << "  'b' in main_workspace: " << keymap.resolve("main_workspace", "b") << "\n";
    std::cout << "  'tab' in main_workspace: " << keymap.resolve("main_workspace", "tab") << "\n";
    std::cout << "  'l' in wizard: " << keymap.resolve("wizard", "l") << "\n";
    std::cout << "  'y' in dialogs: " << keymap.resolve("dialogs", "y") << "\n";

    // Reverse lookup: what key is bound to an action?
    std::cout << "  'quit' is bound to: '" << keymap.findKey("global", "quit") << "'\n";
    std::cout << "  'build' is bound to: '" << keymap.findKey("main_workspace", "build") << "'\n";

    // Load a partial override (user overrides in JSON).
    keymap.loadString(R"([
        {
            "context": "global",
            "bindings": [
                {"key": "Q", "action": "quit"}
            ]
        }
    ])");

    std::cout << "  After override, 'Q' resolves to: "
              << keymap.resolve("global", "Q") << "\n";
    std::cout << "  'q' still works: "
              << keymap.resolve("global", "q") << "\n\n";

    // ======================================================================
    // Part 3: ThemeManager — color themes
    //
    // Themes define color palettes used by the TUI. Built-in themes
    // include "dark" (Tokyo Night inspired), "light", and "nord".
    // User themes can be loaded from JSON files.
    // ======================================================================

    std::cout << "--- ThemeManager ---\n";

    ThemeManager themeManager;

    // List available built-in themes.
    std::cout << "  Available themes: ";
    for (const auto& name : themeManager.availableThemes()) {
        std::cout << name << " ";
    }
    std::cout << "\n";

    // Access the active theme (default: "dark").
    const Theme& dark = themeManager.activeTheme();
    std::cout << "  Active: '" << dark.name << "' (" << dark.description << ")\n";
    std::cout << "    background: " << dark.colors.background << "\n";
    std::cout << "    foreground: " << dark.colors.foreground << "\n";
    std::cout << "    accent: " << dark.colors.accent << "\n";

    // Switch themes.
    themeManager.setActiveTheme("nord");
    const ThemeColors& nord = themeManager.activeColors();
    std::cout << "  Switched to 'nord':\n";
    std::cout << "    background: " << nord.background << "\n";
    std::cout << "    accent: " << nord.accent << "\n";

    // Load a custom theme from a JSON string.
    themeManager.loadString(R"({
        "name": "solarized_dark",
        "description": "Solarized Dark",
        "colors": {
            "background": "#002b36",
            "foreground": "#839496",
            "accent": "#268bd2"
        }
    })");

    const Theme* solarized = themeManager.findTheme("solarized_dark");
    if (solarized) {
        std::cout << "  Loaded custom theme: " << solarized->name << "\n";
    }

    themeManager.setActiveTheme("solarized_dark");
    std::cout << "  Active: " << themeManager.activeTheme().name << "\n\n";

    // ======================================================================
    // Part 4: TemplateManager — project templates
    //
    // Templates define starter project structures with {{placeholder}}
    // substitution. Built-in templates include:
    //   - console_app        — Simple C++ console app
    //   - static_library     — C++ library with header/source
    //   - header_only_library — Header-only C++ library
    //   - c_console_app      — Simple C console app
    //
    // The TemplateManager::render() method substitutes placeholders
    // and returns the rendered file contents.
    // ======================================================================

    std::cout << "--- TemplateManager ---\n";

    TemplateManager templates;

    // List available templates.
    std::cout << "  Available templates: ";
    for (const auto& name : templates.availableTemplates()) {
        std::cout << "\n    - " << name;
    }
    std::cout << "\n\n";

    // Render a template with custom values.
    TemplateContext ctx;
    ctx.projectName = "MyAwesomeGame";
    ctx.projectNamespace = "my_awesome_game";
    ctx.authorName = "Alice";
    ctx.year = "2026";

    auto rendered = templates.render("console_app", ctx);
    if (rendered.has_value()) {
        std::cout << "  Rendered 'console_app' template:\n";
        for (const auto& [path, content] : *rendered) {
            std::cout << "\n    📄 " << path.generic_string() << ":\n";
            // Print first 3 lines of content.
            std::istringstream stream(content);
            std::string line;
            int lineCount = 0;
            while (std::getline(stream, line) && lineCount < 3) {
                std::cout << "      " << line << "\n";
                ++lineCount;
            }
            if (lineCount >= 3) std::cout << "      ...\n";
        }
    }

    // Create and render a custom template at runtime.
    std::cout << "\n  Registering custom template...\n";
    ProjectTemplate custom;
    custom.name = "hello_world";
    custom.description = "A simple hello world program";
    custom.defaultTargetKind = "Executable";
    custom.language = "Cpp";
    custom.files.push_back({
        .sourcePath = "main.cpp",
        .content = R"(// {{project_name}} — by {{author_name}}, {{year}}
#include <iostream>

int main() {
    std::cout << "Hello, {{project_name}}!" << std::endl;
    return 0;
}
)",
    });
    templates.registerTemplate(custom);

    TemplateContext helloCtx;
    helloCtx.projectName = "HelloWorld";
    helloCtx.authorName = "Bob";
    helloCtx.year = "2026";

    auto helloRendered = templates.render("hello_world", helloCtx);
    if (helloRendered.has_value()) {
        std::cout << "    Rendered custom template:\n";
        for (const auto& [path, content] : *helloRendered) {
            std::cout << "      " << path.generic_string() << ": " << content;
        }
    }

    std::cout << "\n=== Done! ===\n";
    return 0;
}
