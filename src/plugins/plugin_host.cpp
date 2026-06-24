#include "lazycmake/plugins/plugin_host.hpp"

#include <fstream>
#include <spdlog/spdlog.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace lazycmake::plugins {

PluginHost::PluginHost(events::EventBus& eventBus)
    : eventBus_(eventBus), pluginBus_(eventBus_) {}

PluginHost::~PluginHost() {
    unloadAll();
}

void PluginHost::setPluginDir(const std::filesystem::path& dir) {
    pluginDir_ = dir;
}

int PluginHost::discoverManifests(const std::filesystem::path& dir) {
    if (!std::filesystem::exists(dir)) return 0;

    int count = 0;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(dir)) {
            if (entry.path().extension() == ".json") {
                try {
                    std::ifstream file(entry.path());
                    auto json = nlohmann::json::parse(file);

                    PluginManifest manifest;
                    manifest.name = json.value("name", entry.path().stem().string());
                    manifest.library = json.value("library", "");
                    manifest.enabled = json.value("enabled", true);
                    manifest.version = json.value("version", "0.1.0");
                    manifest.description = json.value("description", "");

                    if (manifest.library.empty()) {
                        manifest.library = "lib" + manifest.name + ".so";
                    }

                    manifests_.push_back(manifest);
                    count++;
                } catch (const std::exception& e) {
                    spdlog::warn("PluginHost: failed to parse manifest {}: {}",
                                 entry.path().string(), e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        spdlog::error("PluginHost: failed to scan {}: {}", dir.string(), e.what());
    }

    spdlog::info("PluginHost: discovered {} plugin manifest(s)", count);
    return count;
}

int PluginHost::loadEnabled() {
    int loaded = 0;
    for (const auto& manifest : manifests_) {
        if (!manifest.enabled) {
            spdlog::debug("PluginHost: skipping disabled plugin '{}'", manifest.name);
            continue;
        }
        if (loadSingle(manifest)) loaded++;
    }
    return loaded;
}

void PluginHost::unloadAll() {
    for (auto& [name, plugin] : loaded_) {
        unloadSingle(&plugin);
    }
    loaded_.clear();
}

LoadedPlugin* PluginHost::getPlugin(const std::string& name) {
    auto it = loaded_.find(name);
    if (it != loaded_.end()) return &it->second;
    return nullptr;
}

std::vector<LoadedPlugin*> PluginHost::loadedPlugins() {
    std::vector<LoadedPlugin*> result;
    for (auto& [name, plugin] : loaded_) {
        result.push_back(&plugin);
    }
    return result;
}

bool PluginHost::loadSingle(const PluginManifest& manifest) {
    std::filesystem::path libPath = manifest.library;
    if (!libPath.is_absolute() && !pluginDir_.empty()) {
        libPath = pluginDir_ / libPath;
    }

    if (!std::filesystem::exists(libPath)) {
        spdlog::warn("PluginHost: library not found for '{}': {}", manifest.name, libPath.string());
        return false;
    }

#ifdef _WIN32
    auto handle = LoadLibraryW(libPath.wstring().c_str());
    if (!handle) {
        spdlog::error("PluginHost: failed to load '{}': error {}", manifest.name, GetLastError());
        return false;
    }
    auto descriptor = reinterpret_cast<PluginDescriptor*>(
        GetProcAddress(handle, "lazycmake_plugin_descriptor"));
#else
    auto handle = dlopen(libPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        spdlog::error("PluginHost: failed to load '{}': {}", manifest.name, dlerror());
        return false;
    }
    auto descriptor = reinterpret_cast<PluginDescriptor*>(
        dlsym(handle, "lazycmake_plugin_descriptor"));
#endif

    if (!descriptor) {
        spdlog::error("PluginHost: '{}' has no lazycmake_plugin_descriptor symbol", manifest.name);
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return false;
    }

    if (descriptor->apiVersion != LAZYCMAKE_PLUGIN_API_VERSION) {
        spdlog::error("PluginHost: '{}' API version {} != expected {}",
                      manifest.name, descriptor->apiVersion, LAZYCMAKE_PLUGIN_API_VERSION);
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return false;
    }

    auto* instance = descriptor->createPlugin();
    if (!instance) {
        spdlog::error("PluginHost: '{}' createPlugin returned null", manifest.name);
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return false;
    }

    PluginContext ctx{
        .eventBus = pluginBus_,
        .logInfo = [name = manifest.name](const std::string& msg) {
            spdlog::info("[plugin:{}] {}", name, msg);
        },
        .logError = [name = manifest.name](const std::string& msg) {
            spdlog::error("[plugin:{}] {}", name, msg);
        },
        .configDir = pluginDir_.empty() ? "" : (pluginDir_ / manifest.name).string(),
    };

    if (!instance->onLoad(ctx)) {
        spdlog::error("PluginHost: '{}' onLoad failed", manifest.name);
        descriptor->destroyPlugin(instance);
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle));
#else
        dlclose(handle);
#endif
        return false;
    }

    LoadedPlugin loaded;
    loaded.manifest = manifest;
    loaded.instance.reset(instance);
    loaded.libraryHandle = handle;

    loaded_[manifest.name] = std::move(loaded);
    spdlog::info("PluginHost: loaded plugin '{}' v{}", manifest.name, manifest.version);

    eventBus_.publish(events::PluginLoadedEvent{manifest.name, manifest.version});
    return true;
}

void PluginHost::unloadSingle(LoadedPlugin* plugin) {
    if (!plugin) return;
    plugin->instance->onUnload();

    auto* desc = reinterpret_cast<PluginDescriptor*>(
#ifdef _WIN32
        GetProcAddress(static_cast<HMODULE>(plugin->libraryHandle), "lazycmake_plugin_descriptor")
#else
        dlsym(plugin->libraryHandle, "lazycmake_plugin_descriptor")
#endif
    );

    if (desc) {
        desc->destroyPlugin(plugin->instance.release());
    }

#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(plugin->libraryHandle));
#else
    dlclose(plugin->libraryHandle);
#endif

    spdlog::info("PluginHost: unloaded plugin '{}'", plugin->manifest.name);
}

} // namespace lazycmake::plugins
