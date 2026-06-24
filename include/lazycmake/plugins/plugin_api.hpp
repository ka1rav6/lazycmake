#pragma once

#include <functional>
#include <memory>
#include <string>

#include "lazycmake/events/event_bus.hpp"

#define LAZYCMAKE_PLUGIN_API_VERSION 1

namespace lazycmake::plugins {

struct PluginContext {
    events::PluginEventBus& eventBus;
    std::function<void(const std::string&)> logInfo;
    std::function<void(const std::string&)> logError;
    std::string configDir;
};

class IPlugin {
public:
    virtual ~IPlugin() = default;
    virtual std::string name() const = 0;
    virtual std::string version() const = 0;
    virtual bool onLoad(PluginContext& ctx) = 0;
    virtual void onUnload() = 0;
};

extern "C" {
struct PluginDescriptor {
    int apiVersion;
    const char* name;
    const char* version;
    IPlugin* (*createPlugin)();
    void (*destroyPlugin)(IPlugin*);
};

PluginDescriptor lazycmake_plugin_descriptor();
}

} // namespace lazycmake::plugins
