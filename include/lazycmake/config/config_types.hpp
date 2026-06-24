#pragma once

// ==========================================================================
// Config types — shared types for the configuration system (§15)
//
// These types define the shape of each configuration surface (settings,
// keybindings, themes, templates) as plain data structures that can be
// serialized to/from JSON. The managers in this directory load, merge,
// and resolve these types from layered config files.
//
// Layered resolution (§15.1):
//   1. Compiled-in defaults (resources/ embedded in the binary)
//   2. System-wide config (/etc/lazycmake/)
//   3. User config (~/.config/lazycmake/)
//   4. Project-local overrides (<project>/.lazycmake/)
//   5. CLI flags / env vars
//
// Each layer is a partial override — listing only what changes. Deep
// merging (not replacement) is used for maps, while arrays are replaced
// wholesale. This matches Git/ripgrep-style config behavior.
// ==========================================================================

#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lazycmake::config {

// ==========================================================================
// Settings — application-wide settings
//
// These map to settings you'd find in `settings.toml` / `settings.json`.
// Each field has a built-in default; user config files only need to
// override the fields they want to change.
// ==========================================================================

struct BuildSettings {
    // Number of parallel build jobs (0 = let Ninja/CMake decide).
    int parallelJobs = 0;

    // Whether to export compile_commands.json.
    bool exportCompileCommands = true;

    // Whether to run CMake configure before each build if the project
    // model has changed.
    bool autoConfigure = true;

    // Build timeout in seconds (0 = no timeout).
    int buildTimeoutSec = 0;

    // Whether to show full compiler command lines (vs. brief output).
    bool verboseBuild = false;
};

struct EditorSettings {
    // External editor command (e.g. "vim", "code", "nano").
    // Empty = use the EDITOR/VISUAL environment variable.
    std::string editorCommand;

    // Whether to auto-save before running the external editor.
    bool autoSaveBeforeEdit = true;

    // File pattern for syntax highlighting preview (future).
    std::vector<std::string> highlightExtensions = {".cpp", ".hpp", ".h", ".c", ".hpp"};
};

struct GeneralSettings {
    // UI language/locale (empty = system default).
    std::string language = "en";

    // Whether to check for updates on startup.
    bool checkUpdates = false;

    // Whether to restore the last opened project on startup.
    bool restoreLastProject = true;

    // Confirm before destructive actions (delete target, remove file, etc.).
    bool confirmDestructiveActions = true;
};

struct AppSettings {
    GeneralSettings general;
    BuildSettings build;
    EditorSettings editor;

    // Whether to flatten (deep-merge) or replace when merging arrays.
    // True = merge array values additively; False = replace entirely.
    bool deepMergeArrays = false;
};

// ==========================================================================
// Keymap — keybinding definitions
//
// An Action is a semantic command that the TUI layer dispatches.
// KeymapManager resolves a raw key event (e.g. "b", "ctrl+q", "tab")
// into an Action string. Each panel context (global, main_workspace,
// wizard, settings_screen) has its own namespace of bindings.
//
// We store actions as strings rather than an enum so that plugins can
// register new actions without requiring a core enum change.
// ==========================================================================

// A single keybinding: maps a keypress description to an action name.
struct KeyBinding {
    std::string key;         // e.g. "q", "ctrl+c", "tab", "shift+tab", "f1"
    std::string action;      // e.g. "quit", "help", "build", "run", "next_panel"
};

// A set of bindings for a given context/panel.
struct KeymapContext {
    std::string context;            // e.g. "global", "main_workspace", "wizard"
    std::vector<KeyBinding> bindings;
};

// The complete keymap, consisting of multiple contexts.
struct Keymap {
    // Default contexts are pre-populated from compiled-in defaults.
    std::vector<KeymapContext> contexts;

    // Find a binding for a given action in a given context.
    // Returns empty string if not found.
    [[nodiscard]] std::string findKey(const std::string& context,
                                       const std::string& action) const;

    // Find the action bound to a given key in a given context.
    // Returns empty string if not found.
    [[nodiscard]] std::string findAction(const std::string& context,
                                          const std::string& key) const;

    // Add or update a binding in a context.
    void setBinding(const std::string& context,
                    const std::string& key,
                    const std::string& action);
};

// ==========================================================================
// Theme — color theme definitions
//
// A theme defines a palette used by the TUI layer. Each panel component
// pulls its colors from this struct rather than hardcoding them, which
// is what makes themes work without per-component changes (§14).
//
// Colors are stored as strings in "#RRGGBB" hex format, or named FTXUI
// color strings. The ThemeManager resolves these to FTXUI Color values.
// ==========================================================================

struct ThemeColors {
    // General UI
    std::string background = "#1a1b26";         // Default dark bg (Nord-like)
    std::string foreground = "#a9b1d6";         // Default text
    std::string accent = "#7aa2f7";             // Accent/selection
    std::string accentSecondary = "#bb9af7";    // Secondary accent

    // Semantic colors
    std::string success = "#9ece6a";            // Green for success states
    std::string warning = "#e0af68";            // Yellow/orange for warnings
    std::string error = "#f7768e";              // Red for errors
    std::string info = "#2ac3de";               // Cyan for info messages

    // Panel-specific
    std::string panelBorderFocused = "#7aa2f7";
    std::string panelBorderUnfocused = "#565f89";
    std::string panelTitle = "#a9b1d6";
    std::string panelBackground = "#1a1b26";

    // List/table
    std::string listSelectionBackground = "#3b4261";
    std::string listSelectionForeground = "#c0caf5";
    std::string listInactiveBackground = "#1a1b26";
    std::string listInactiveForeground = "#565f89";
};

struct Theme {
    std::string name = "dark";
    std::string description = "Default dark theme (Tokyo Night inspired)";
    ThemeColors colors;
};

// ==========================================================================
// Template — project template metadata
//
// A template is a directory containing:
//   template.json     — metadata (this struct serialized)
//   src/main.cpp.tmpl — Mustache-style template files
//   include/          — additional directory structure
//
// The TemplateManager discovers templates from built-in resources and
// user directories, merging them by name (user templates override built-in).
// ==========================================================================

struct TemplateFile {
    std::filesystem::path sourcePath;    // relative path within the template
    std::string content;                 // template content with {{placeholders}}
};

struct ProjectTemplate {
    std::string name;                    // unique name
    std::string description;             // human-readable description
    std::vector<std::string> defaultDependencies;   // dependency names to pre-enable
    std::string defaultTargetKind;       // "Executable", "StaticLibrary", etc.
    std::vector<std::string> defaultSourceFiles;    // e.g. ["src/main.cpp"]
    std::string language;                // "C", "Cpp", "Both"

    // The template files with {{placeholder}} substitutions.
    std::vector<TemplateFile> files;
};

// ==========================================================================
// JSON (de)serialization hooks for all config types
// ==========================================================================

void to_json(nlohmann::json& j, const BuildSettings& s);
void from_json(const nlohmann::json& j, BuildSettings& s);

void to_json(nlohmann::json& j, const EditorSettings& s);
void from_json(const nlohmann::json& j, EditorSettings& s);

void to_json(nlohmann::json& j, const GeneralSettings& s);
void from_json(const nlohmann::json& j, GeneralSettings& s);

void to_json(nlohmann::json& j, const AppSettings& s);
void from_json(const nlohmann::json& j, AppSettings& s);

void to_json(nlohmann::json& j, const KeyBinding& b);
void from_json(const nlohmann::json& j, KeyBinding& b);

void to_json(nlohmann::json& j, const KeymapContext& c);
void from_json(const nlohmann::json& j, KeymapContext& c);

void to_json(nlohmann::json& j, const Keymap& k);
void from_json(const nlohmann::json& j, Keymap& k);

void to_json(nlohmann::json& j, const ThemeColors& c);
void from_json(const nlohmann::json& j, ThemeColors& c);

void to_json(nlohmann::json& j, const Theme& t);
void from_json(const nlohmann::json& j, Theme& t);

void to_json(nlohmann::json& j, const ProjectTemplate& t);
void from_json(const nlohmann::json& j, ProjectTemplate& t);

} // namespace lazycmake::config
