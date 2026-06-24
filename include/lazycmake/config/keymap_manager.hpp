#pragma once

// ==========================================================================
// KeymapManager — resolves key events to semantic actions (§15.2)
//
// The TUI layer never matches on raw key events directly. Instead, every
// component passes the raw event to KeymapManager::resolve(), which looks
// up the current context (e.g. "main_workspace", "wizard") and returns the
// semantic action string (e.g. "build", "quit", "next_panel").
//
// Why this matters (§14):
//   - Key rebinding is trivial: a user edits their keymap.toml, which only
//     needs to list the few bindings they want to change. Everything else
//     falls through to the compiled-in defaults.
//   - The Help overlay can query "what keys are bound to X" from one place.
//   - A plugin can register new contexts and bindings.
//
// Like SettingsManager, this implements layered resolution:
//   Built-in defaults → system keymap → user keymap → project keymap.
// ==========================================================================

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lazycmake/config/config_types.hpp"

namespace lazycmake::config {

class KeymapManager {
public:
    KeymapManager();

    // Load keymap from all layers and merge.
    // Returns the number of layers loaded.
    int loadAll();

    // Load a single keymap file.
    bool loadFile(const std::filesystem::path& path);

    // Load a keymap from a JSON string.
    bool loadString(const std::string& jsonString);

    // Resolve a key to an action in the given context.
    // Returns the action name string, or empty string if unbound.
    // Example: resolve("main_workspace", "b") -> "build"
    [[nodiscard]] std::string resolve(const std::string& context,
                                       const std::string& key) const;

    // Find what key is bound to an action in a context.
    // Used by the Help overlay to display current bindings.
    // Example: findKey("global", "quit") -> "q"
    [[nodiscard]] std::string findKey(const std::string& context,
                                       const std::string& action) const;

    // Get all bindings for a given context (for display in Help overlay).
    [[nodiscard]] std::vector<KeyBinding> getBindings(const std::string& context) const;

    // Get the merged keymap.
    [[nodiscard]] const Keymap& keymap() const;

    // Check if any keymap file has changed.
    [[nodiscard]] bool hasChanged() const;

    // Reset to compiled-in defaults.
    void resetToDefaults();

    // List all available contexts.
    [[nodiscard]] std::vector<std::string> contexts() const;

private:
    Keymap keymap_;

    std::filesystem::path systemConfigDir_;
    std::filesystem::path userConfigDir_;

    bool loadLayer(const std::filesystem::path& path);

    // Build the default keymap with vim-like bindings.
    // These are the compiled-in defaults that ship with the binary.
    static Keymap buildDefaultKeymap();
};

} // namespace lazycmake::config
