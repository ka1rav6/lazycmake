#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"

namespace lazycmake::tui {

// File panel — left-most panel showing project file tree.
// Phase 5: static dummy file list. Real fs integration in Phase 6.
class FilePanel {
public:
    explicit FilePanel(config::KeymapManager& keymap,
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
    std::vector<std::string> files_;
    int selectedIndex_ = 0;
};

} // namespace lazycmake::tui
