#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/project.hpp"
#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// Lightweight FTXUI component that delegates Render/OnEvent to whatever
// Component its pointer targets. This lets us swap the entire screen
// without rebuilding the root component that ScreenInteractive::Loop()
// holds onto.
class ScreenHolder : public ftxui::ComponentBase {
public:
    explicit ScreenHolder(ftxui::Component* target) : target_(target) {}

    ftxui::Element Render() override {
        if (target_ && *target_) return (*target_)->Render();
        return ftxui::emptyElement();
    }

    bool OnEvent(ftxui::Event event) override {
        if (target_ && *target_) return (*target_)->OnEvent(event);
        return false;
    }

private:
    ftxui::Component* target_;
};

class StartupScreen;

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
    ftxui::Component makeWizardScreen();
    ftxui::Component makeSettingsScreen();
    ftxui::Component makeWorkspaceScreen(core::Project project);
    void onStartupAction(int idx);
    void navigateToStartup();
    void navigateToWizard();
    void navigateToSettings();
    void navigateToWorkspace(core::Project project);
    void onGenerateProject(core::Project project);

    events::EventBus eventBus_;
    config::SettingsManager settings_;
    config::KeymapManager keymap_;
    config::ThemeManager theme_;
    ftxui::ScreenInteractive screen_;

    ftxui::Component currentScreen_;
    ftxui::Component screenHolder_;
};

} // namespace lazycmake::tui
