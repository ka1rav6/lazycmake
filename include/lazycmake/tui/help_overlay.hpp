#pragma once

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"

namespace lazycmake::tui {

// Help overlay showing keybindings grouped by context.
class HelpOverlay {
public:
    explicit HelpOverlay(config::KeymapManager& keymap);

    ftxui::Component build();
    void show();
    void hide();
    bool isVisible() const { return visible_; }
    void toggle();

private:
    config::KeymapManager& keymap_;
    bool visible_ = false;
    ftxui::Component overlay_;
};

} // namespace lazycmake::tui
