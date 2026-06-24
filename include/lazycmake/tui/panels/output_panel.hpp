#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"

namespace lazycmake::tui {

// Output panel — right-most panel showing build/run/log output.
// Phase 5: static placeholder. Real streaming output in Phase 6.
class OutputPanel {
public:
    explicit OutputPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme);

    ftxui::Component build();
    void focus() { focused_ = true; }
    void blur() { focused_ = false; }

    // Append a line of output (Phase 6: called by EventBus subscriber).
    void append(const std::string& line);

private:
    bool onEvent(ftxui::Event event);
    ftxui::Element render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    bool focused_ = false;

    // In-memory log buffer.
    std::vector<std::string> lines_;
    int scrollOffset_ = 0;
};

} // namespace lazycmake::tui
