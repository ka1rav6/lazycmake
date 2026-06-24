#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"

namespace lazycmake::tui {

// Target panel — center panel showing project targets.
// Phase 5: static dummy target list. Real Project-backed list in Phase 6.
class TargetPanel {
public:
    explicit TargetPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme);

    ftxui::Component build();
    void focus() { focused_ = true; }
    void blur() { focused_ = false; }

private:
    bool onEvent(ftxui::Event event);
    ftxui::Element render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    bool focused_ = false;

    // Static dummy data for Phase 5.
    std::vector<std::string> targets_;
    int selectedIndex_ = 0;
};

} // namespace lazycmake::tui
