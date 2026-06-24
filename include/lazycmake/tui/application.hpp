#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/screen_interactive.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

class Application {
public:
    Application();
    ~Application();

    int run(int argc, char** argv);

    events::EventBus& eventBus() { return eventBus_; }
    config::SettingsManager& settings() { return settings_; }
    config::KeymapManager& keymap() { return keymap_; }
    config::ThemeManager& theme() { return theme_; }
    ScreenStack& screens() { return screens_; }

private:
    void loadConfig();
    ftxui::Component buildRootComponent();
    ftxui::Component buildMainComponent();
    bool onEvent(ftxui::Event event);

    events::EventBus eventBus_;
    config::SettingsManager settings_;
    config::KeymapManager keymap_;
    config::ThemeManager theme_;
    ScreenStack screens_;
    ftxui::ScreenInteractive screen_;
};

} // namespace lazycmake::tui
