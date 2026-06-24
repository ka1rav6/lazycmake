#pragma once

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// Settings screen showing config categories.
class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(config::KeymapManager& keymap,
                            config::SettingsManager& settings);
    ~SettingsScreen() override = default;

private:
    ftxui::Component buildSettingsView();
    bool onEvent(ftxui::Event event);

    config::KeymapManager& keymap_;
    config::SettingsManager& settings_;
};

} // namespace lazycmake::tui
