#pragma once

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// Startup screen with menu: New, Open, Recent, Templates, Settings, Quit.
class StartupScreen : public Screen {
public:
    explicit StartupScreen(config::KeymapManager& keymap);
    ~StartupScreen() override = default;

    int selectedIndex() const { return selectedIndex_; }

private:
    ftxui::Component buildMenu();
    bool onEvent(ftxui::Event event);

    config::KeymapManager& keymap_;
    int selectedIndex_ = 0;
    ftxui::Component menu_;
};

} // namespace lazycmake::tui
