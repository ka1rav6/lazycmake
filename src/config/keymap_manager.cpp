#include "lazycmake/config/keymap_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <spdlog/spdlog.h>

namespace lazycmake::config {

KeymapManager::KeymapManager() {
    const char* home = std::getenv("HOME");
    if (home) {
        userConfigDir_ = std::filesystem::path(home) / ".config" / "lazycmake";
    }
    // Initialize with built-in default keymap so the manager is always
    // usable immediately after construction, even if no config files exist.
    resetToDefaults();
}

int KeymapManager::loadAll() {
    resetToDefaults();
    int layersLoaded = 0;

    // Load system keymap (/etc/lazycmake/keymap.json).
    if (loadLayer(std::filesystem::path("/etc/lazycmake") / "keymap.json")) {
        ++layersLoaded;
    }

    // Load user keymap (~/.config/lazycmake/keymap.json).
    if (!userConfigDir_.empty()) {
        if (loadLayer(userConfigDir_ / "keymap.json")) {
            ++layersLoaded;
        }
    }

    return layersLoaded;
}

bool KeymapManager::loadFile(const std::filesystem::path& path) {
    return loadLayer(path);
}

bool KeymapManager::loadString(const std::string& jsonString) {
    try {
        nlohmann::json j = nlohmann::json::parse(jsonString);
        Keymap layerKeymap = j.get<Keymap>();

        // Merge: for each context in the layer, overlay its bindings
        // onto the existing keymap. Existing bindings for the same key
        // are overridden; new bindings are added.
        for (const auto& ctx : layerKeymap.contexts) {
            for (const auto& binding : ctx.bindings) {
                keymap_.setBinding(ctx.context, binding.key, binding.action);
            }
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("KeymapManager: Failed to load keymap: {}", e.what());
        return false;
    }
}

std::string KeymapManager::resolve(const std::string& context,
                                    const std::string& key) const {
    return keymap_.findAction(context, key);
}

std::string KeymapManager::findKey(const std::string& context,
                                    const std::string& action) const {
    return keymap_.findKey(context, action);
}

std::vector<KeyBinding> KeymapManager::getBindings(const std::string& context) const {
    for (const auto& ctx : keymap_.contexts) {
        if (ctx.context == context) {
            return ctx.bindings;
        }
    }
    return {};
}

const Keymap& KeymapManager::keymap() const {
    return keymap_;
}

bool KeymapManager::hasChanged() const {
    // Placeholder: would need FileWatcher integration (Phase 6) for proper
    // hot-reload of keymap files. For now, always returns false.
    (void)userConfigDir_;
    return false;
}

void KeymapManager::resetToDefaults() {
    keymap_ = buildDefaultKeymap();
}

std::vector<std::string> KeymapManager::contexts() const {
    std::vector<std::string> result;
    for (const auto& ctx : keymap_.contexts) {
        result.push_back(ctx.context);
    }
    return result;
}

bool KeymapManager::loadLayer(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return false;
    }

    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        nlohmann::json j;
        file >> j;
        Keymap layerKeymap = j.get<Keymap>();

        for (const auto& ctx : layerKeymap.contexts) {
            for (const auto& binding : ctx.bindings) {
                keymap_.setBinding(ctx.context, binding.key, binding.action);
            }
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("KeymapManager: Error loading {}: {}", path.string(), e.what());
        return false;
    }
}

Keymap KeymapManager::buildDefaultKeymap() {
    // These are the compiled-in vim-like defaults.
    // They ship with the binary and are always the first (lowest-priority)
    // layer in the resolution chain.
    //
    // The convention:
    //   global:            keys that work everywhere (quit, help)
    //   main_workspace:    keys for the 3-panel Files/Targets/Output view
    //   main_workspace.panels: panel navigation keys
    //   wizard:            keys for the New Project Wizard steps
    //   build_overlay:     keys for the Build overlay
    //   run_overlay:       keys for the Run overlay
    //   dialogs:           keys for confirmation dialogs
    Keymap km;

    km.contexts.push_back({"global", {
        {"q", "quit"},
        {"h", "help"},
        {"?", "help"},
        {"s", "settings"},
        {"ctrl+c", "interrupt"},
        {"esc", "close_overlay"},
    }});

    km.contexts.push_back({"main_workspace", {
        {"b", "build"},
        {"r", "run"},
        {"c", "configure"},
        {"g", "generate"},
        {"n", "new_target"},
        {"d", "dependencies"},
        {"tab", "next_panel"},
        {"shift+tab", "previous_panel"},
        {"enter", "open"},
        {"/", "filter"},
        {"R", "refresh"},
        {"x", "delete"},
        {"e", "edit_file"},
    }});

    km.contexts.push_back({"wizard", {
        {"l", "next"},
        {"h", "previous"},
        {"enter", "select"},
        {"esc", "cancel"},
        {"tab", "next_field"},
        {"shift+tab", "previous_field"},
    }});

    km.contexts.push_back({"build_overlay", {
        {"b", "build"},
        {"c", "configure"},
        {"g", "generate"},
        {"r", "run"},
        {"esc", "close"},
        {"ctrl+c", "cancel_build"},
    }});

    km.contexts.push_back({"run_overlay", {
        {"r", "run"},
        {"k", "kill"},
        {"esc", "close"},
    }});

    km.contexts.push_back({"dialogs", {
        {"y", "yes"},
        {"n", "no"},
        {"enter", "confirm"},
        {"esc", "cancel"},
    }});

    return km;
}

} // namespace lazycmake::config
