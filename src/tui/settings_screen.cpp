#include "lazycmake/tui/settings_screen.hpp"

#include <sstream>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

SettingsScreen::SettingsScreen(config::KeymapManager& keymap,
                               config::SettingsManager& settings)
    : keymap_(keymap), settings_(settings) {
    name = "settings";
    component = buildSettingsView();
}

ftxui::Component SettingsScreen::buildSettingsView() {
    auto self = this;
    return ftxui::Renderer(std::function<ftxui::Element()>([self] {
        const auto& app = self->settings_.settings();

        auto lines = ftxui::vbox({
            ftxui::text("=== General Settings ==="),
            ftxui::text("  Language:    " + app.general.language),
            ftxui::text("  Check Updates: " + std::string(app.general.checkUpdates ? "yes" : "no")),
            ftxui::text("  Restore Last Project: " + std::string(app.general.restoreLastProject ? "yes" : "no")),
            ftxui::text(""),
            ftxui::text("=== Build Settings ==="),
            ftxui::text("  Parallel Jobs: " + std::to_string(app.build.parallelJobs)),
            ftxui::text("  Export Compile Commands: " + std::string(app.build.exportCompileCommands ? "yes" : "no")),
            ftxui::text("  Auto Configure: " + std::string(app.build.autoConfigure ? "yes" : "no")),
            ftxui::text("  Build Timeout: " + std::to_string(app.build.buildTimeoutSec) + "s"),
            ftxui::text("  Verbose Build: " + std::string(app.build.verboseBuild ? "yes" : "no")),
            ftxui::text(""),
            ftxui::text("=== Editor Settings ==="),
            ftxui::text("  Editor Command: " + (app.editor.editorCommand.empty() ? "(use $EDITOR)" : app.editor.editorCommand)),
            ftxui::text("  Auto Save Before Edit: " + std::string(app.editor.autoSaveBeforeEdit ? "yes" : "no")),
            ftxui::text(""),
            ftxui::text("Press 'q' or 'esc' to go back."),
        });

        return lines | ftxui::border;
    }));
}

} // namespace lazycmake::tui
