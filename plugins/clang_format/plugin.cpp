#include <cstring>
#include <iostream>
#include <string>

#include "lazycmake/plugins/plugin_api.hpp"

using namespace lazycmake::plugins;

class ClangFormatPlugin : public IPlugin {
public:
    std::string name() const override { return "clang-format"; }
    std::string version() const override { return "1.0.0"; }

    bool onLoad(PluginContext& ctx) override {
        ctx.logInfo("clang-format plugin loaded");
        ctx.logInfo("config dir: " + ctx.configDir);

        auto id = ctx.eventBus.subscribe<lazycmake::events::FileAddedEvent>(
            [](const auto& event) {
                std::cout << "[clang-format] New file detected: "
                          << event.path << std::endl;
            });

        ctx.logInfo("subscribed to FileAddedEvent");
        return true;
    }

    void onUnload() override {
        std::cout << "[clang-format] plugin unloaded" << std::endl;
    }
};

extern "C" {

PluginDescriptor lazycmake_plugin_descriptor() {
    return PluginDescriptor{
        .apiVersion = LAZYCMAKE_PLUGIN_API_VERSION,
        .name = "clang-format",
        .version = "1.0.0",
        .createPlugin = []() -> IPlugin* { return new ClangFormatPlugin(); },
        .destroyPlugin = [](IPlugin* p) { delete p; },
    };
}

} // extern "C"
