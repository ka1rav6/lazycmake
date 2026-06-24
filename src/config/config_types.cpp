#include "lazycmake/config/config_types.hpp"

#include <algorithm>

namespace lazycmake::config {

// ==========================================================================
// Keymap methods
// ==========================================================================

std::string Keymap::findKey(const std::string& context,
                             const std::string& action) const {
    for (const auto& ctx : contexts) {
        if (ctx.context == context) {
            for (const auto& binding : ctx.bindings) {
                if (binding.action == action) {
                    return binding.key;
                }
            }
        }
    }
    return {};
}

std::string Keymap::findAction(const std::string& context,
                                const std::string& key) const {
    for (const auto& ctx : contexts) {
        if (ctx.context == context) {
            for (const auto& binding : ctx.bindings) {
                if (binding.key == key) {
                    return binding.action;
                }
            }
        }
    }
    return {};
}

void Keymap::setBinding(const std::string& context,
                         const std::string& key,
                         const std::string& action) {
    // Find the context, or create it.
    for (auto& ctx : contexts) {
        if (ctx.context == context) {
            // Find existing binding for this key, or add new.
            for (auto& binding : ctx.bindings) {
                if (binding.key == key) {
                    binding.action = action;
                    return;
                }
            }
            ctx.bindings.push_back({key, action});
            return;
        }
    }
    // Context not found; create it.
    contexts.push_back({context, {{key, action}}});
}

// ==========================================================================
// JSON serialization for all config types
// ==========================================================================

void to_json(nlohmann::json& j, const BuildSettings& s) {
    j = nlohmann::json{
        {"parallelJobs", s.parallelJobs},
        {"exportCompileCommands", s.exportCompileCommands},
        {"autoConfigure", s.autoConfigure},
        {"buildTimeoutSec", s.buildTimeoutSec},
        {"verboseBuild", s.verboseBuild},
    };
}

void from_json(const nlohmann::json& j, BuildSettings& s) {
    if (j.contains("parallelJobs")) j.at("parallelJobs").get_to(s.parallelJobs);
    if (j.contains("exportCompileCommands")) j.at("exportCompileCommands").get_to(s.exportCompileCommands);
    if (j.contains("autoConfigure")) j.at("autoConfigure").get_to(s.autoConfigure);
    if (j.contains("buildTimeoutSec")) j.at("buildTimeoutSec").get_to(s.buildTimeoutSec);
    if (j.contains("verboseBuild")) j.at("verboseBuild").get_to(s.verboseBuild);
}

void to_json(nlohmann::json& j, const EditorSettings& s) {
    j = nlohmann::json{
        {"editorCommand", s.editorCommand},
        {"autoSaveBeforeEdit", s.autoSaveBeforeEdit},
        {"highlightExtensions", s.highlightExtensions},
    };
}

void from_json(const nlohmann::json& j, EditorSettings& s) {
    if (j.contains("editorCommand")) j.at("editorCommand").get_to(s.editorCommand);
    if (j.contains("autoSaveBeforeEdit")) j.at("autoSaveBeforeEdit").get_to(s.autoSaveBeforeEdit);
    if (j.contains("highlightExtensions")) j.at("highlightExtensions").get_to(s.highlightExtensions);
}

void to_json(nlohmann::json& j, const GeneralSettings& s) {
    j = nlohmann::json{
        {"language", s.language},
        {"checkUpdates", s.checkUpdates},
        {"restoreLastProject", s.restoreLastProject},
        {"confirmDestructiveActions", s.confirmDestructiveActions},
    };
}

void from_json(const nlohmann::json& j, GeneralSettings& s) {
    if (j.contains("language")) j.at("language").get_to(s.language);
    if (j.contains("checkUpdates")) j.at("checkUpdates").get_to(s.checkUpdates);
    if (j.contains("restoreLastProject")) j.at("restoreLastProject").get_to(s.restoreLastProject);
    if (j.contains("confirmDestructiveActions")) j.at("confirmDestructiveActions").get_to(s.confirmDestructiveActions);
}

void to_json(nlohmann::json& j, const AppSettings& s) {
    j = nlohmann::json{
        {"general", s.general},
        {"build", s.build},
        {"editor", s.editor},
    };
}

void from_json(const nlohmann::json& j, AppSettings& s) {
    if (j.contains("general")) j.at("general").get_to(s.general);
    if (j.contains("build")) j.at("build").get_to(s.build);
    if (j.contains("editor")) j.at("editor").get_to(s.editor);
}

void to_json(nlohmann::json& j, const KeyBinding& b) {
    j = nlohmann::json{
        {"key", b.key},
        {"action", b.action},
    };
}

void from_json(const nlohmann::json& j, KeyBinding& b) {
    j.at("key").get_to(b.key);
    j.at("action").get_to(b.action);
}

void to_json(nlohmann::json& j, const KeymapContext& c) {
    j = nlohmann::json{
        {"context", c.context},
        {"bindings", c.bindings},
    };
}

void from_json(const nlohmann::json& j, KeymapContext& c) {
    j.at("context").get_to(c.context);
    j.at("bindings").get_to(c.bindings);
}

void to_json(nlohmann::json& j, const Keymap& k) {
    j = nlohmann::json::array();
    for (const auto& ctx : k.contexts) {
        j.push_back(ctx);
    }
}

void from_json(const nlohmann::json& j, Keymap& k) {
    k.contexts.clear();
    for (const auto& elem : j) {
        k.contexts.push_back(elem.get<KeymapContext>());
    }
}

void to_json(nlohmann::json& j, const ThemeColors& c) {
    j = nlohmann::json{
        {"background", c.background},
        {"foreground", c.foreground},
        {"accent", c.accent},
        {"accentSecondary", c.accentSecondary},
        {"success", c.success},
        {"warning", c.warning},
        {"error", c.error},
        {"info", c.info},
        {"panelBorderFocused", c.panelBorderFocused},
        {"panelBorderUnfocused", c.panelBorderUnfocused},
        {"panelTitle", c.panelTitle},
        {"panelBackground", c.panelBackground},
        {"listSelectionBackground", c.listSelectionBackground},
        {"listSelectionForeground", c.listSelectionForeground},
        {"listInactiveBackground", c.listInactiveBackground},
        {"listInactiveForeground", c.listInactiveForeground},
    };
}

void from_json(const nlohmann::json& j, ThemeColors& c) {
    if (j.contains("background")) j.at("background").get_to(c.background);
    if (j.contains("foreground")) j.at("foreground").get_to(c.foreground);
    if (j.contains("accent")) j.at("accent").get_to(c.accent);
    if (j.contains("accentSecondary")) j.at("accentSecondary").get_to(c.accentSecondary);
    if (j.contains("success")) j.at("success").get_to(c.success);
    if (j.contains("warning")) j.at("warning").get_to(c.warning);
    if (j.contains("error")) j.at("error").get_to(c.error);
    if (j.contains("info")) j.at("info").get_to(c.info);
    if (j.contains("panelBorderFocused")) j.at("panelBorderFocused").get_to(c.panelBorderFocused);
    if (j.contains("panelBorderUnfocused")) j.at("panelBorderUnfocused").get_to(c.panelBorderUnfocused);
    if (j.contains("panelTitle")) j.at("panelTitle").get_to(c.panelTitle);
    if (j.contains("panelBackground")) j.at("panelBackground").get_to(c.panelBackground);
    if (j.contains("listSelectionBackground")) j.at("listSelectionBackground").get_to(c.listSelectionBackground);
    if (j.contains("listSelectionForeground")) j.at("listSelectionForeground").get_to(c.listSelectionForeground);
    if (j.contains("listInactiveBackground")) j.at("listInactiveBackground").get_to(c.listInactiveBackground);
    if (j.contains("listInactiveForeground")) j.at("listInactiveForeground").get_to(c.listInactiveForeground);
}

void to_json(nlohmann::json& j, const Theme& t) {
    j = nlohmann::json{
        {"name", t.name},
        {"description", t.description},
        {"colors", t.colors},
    };
}

void from_json(const nlohmann::json& j, Theme& t) {
    j.at("name").get_to(t.name);
    if (j.contains("description")) j.at("description").get_to(t.description);
    if (j.contains("colors")) j.at("colors").get_to(t.colors);
}

void to_json(nlohmann::json& j, const ProjectTemplate& t) {
    j = nlohmann::json{
        {"name", t.name},
        {"description", t.description},
        {"defaultDependencies", t.defaultDependencies},
        {"defaultTargetKind", t.defaultTargetKind},
        {"defaultSourceFiles", t.defaultSourceFiles},
        {"language", t.language},
    };
}

void from_json(const nlohmann::json& j, ProjectTemplate& t) {
    j.at("name").get_to(t.name);
    if (j.contains("description")) j.at("description").get_to(t.description);
    if (j.contains("defaultDependencies")) j.at("defaultDependencies").get_to(t.defaultDependencies);
    if (j.contains("defaultTargetKind")) j.at("defaultTargetKind").get_to(t.defaultTargetKind);
    if (j.contains("defaultSourceFiles")) j.at("defaultSourceFiles").get_to(t.defaultSourceFiles);
    if (j.contains("language")) j.at("language").get_to(t.language);
}

} // namespace lazycmake::config
