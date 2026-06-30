#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/build/build_backend.hpp"
#include "lazycmake/build/build_manager.hpp"
#include "lazycmake/build/run_manager.hpp"
#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/dependency_registry.hpp"
#include "lazycmake/core/project.hpp"
#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/tui/build_overlay.hpp"
#include "lazycmake/tui/conflict_dialog.hpp"
#include "lazycmake/tui/dependency_dialog.hpp"
#include "lazycmake/tui/help_overlay.hpp"
#include "lazycmake/tui/run_overlay.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

struct OverlayState {
    std::shared_ptr<BuildOverlay> buildOverlay;
    std::shared_ptr<RunOverlay> runOverlay;
    std::shared_ptr<DependencyDialog> depDialog;
    std::shared_ptr<HelpOverlay> helpOverlay;
    std::shared_ptr<ConflictDialog> conflictDialog;
};

class Application {
public:
    Application();
    ~Application();

    int run(int argc, char** argv);

    events::EventBus& eventBus() { return eventBus_; }
    config::SettingsManager& settings() { return settings_; }
    config::KeymapManager& keymap() { return keymap_; }
    config::ThemeManager& theme() { return theme_; }

private:
    void loadConfig();
    ftxui::Component makeStartupScreen();

    void onStartupAction(int idx);
    void navigateToStartup();
    void navigateToWizard();
    void navigateToOpen();
    void navigateToTemplates();
    void navigateToSettings();
    void navigateToWorkspace(core::Project project);
    void onGenerateProject(core::Project project);

    void setScreen(ftxui::Component screen);

    events::EventBus eventBus_;
    config::SettingsManager settings_;
    config::KeymapManager keymap_;
    config::ThemeManager theme_;
    core::DependencyRegistry dependencyRegistry_;
    core::Project project_;
    ftxui::ScreenInteractive screen_;
    build::BuildManager buildManager_;
    build::RunManager runManager_;

    // Root FTXUI container.  Passed once to Loop(); we swap children
    // via DetachAllChildren() + Add() to transition between screens.
    ftxui::Component screenRoot_;

    // Holds the current Screen object alive so that lambdas capturing
    // |this| (raw pointer to the screen) remain valid.
    std::unique_ptr<Screen> screenState_;

    // Holds overlay state alive for the workspace.
    std::unique_ptr<OverlayState> overlayState_;
};

} // namespace lazycmake::tui
