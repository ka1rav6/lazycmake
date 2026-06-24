#pragma once

#include <functional>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// Startup screen with menu: New, Open, Recent, Templates, Settings, Quit.
class StartupScreen : public Screen {
public:
    explicit StartupScreen(config::KeymapManager& keymap);
    ~StartupScreen() override = default;

    using ActionCallback = std::function<void(int)>;
    void setOnAction(ActionCallback cb) { onAction_ = std::move(cb); }

    int selectedIndex() const { return selectedIndex_; }

private:
    ftxui::Component buildMenu();

    config::KeymapManager& keymap_;
    int selectedIndex_ = 0;
    ftxui::Component menu_;
    ActionCallback onAction_;
};

} // namespace lazycmake::tui
