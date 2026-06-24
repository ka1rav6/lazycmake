#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/plugins/plugin_api.hpp"

namespace lazycmake::plugins {

struct PluginManifest {
    std::string name;
    std::string library;
    bool enabled = true;
    std::string version;
    std::string description;
};

struct LoadedPlugin {
    PluginManifest manifest;
    std::unique_ptr<IPlugin> instance;
    void* libraryHandle = nullptr;
};

class PluginHost {
public:
    explicit PluginHost(events::EventBus& eventBus);
    ~PluginHost();

    int discoverManifests(const std::filesystem::path& dir);
    int loadEnabled();
    void unloadAll();

    LoadedPlugin* getPlugin(const std::string& name);
    std::vector<LoadedPlugin*> loadedPlugins();

    void setPluginDir(const std::filesystem::path& dir);

private:
    bool loadSingle(const PluginManifest& manifest);
    void unloadSingle(LoadedPlugin* plugin);

    events::EventBus& eventBus_;
    events::PluginEventBus pluginBus_;
    std::filesystem::path pluginDir_;
    std::vector<PluginManifest> manifests_;
    std::map<std::string, LoadedPlugin> loaded_;
};

} // namespace lazycmake::plugins
