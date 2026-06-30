#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/tui/panels/panel_base.hpp"

namespace lazycmake::tui {

// Output panel — right-most panel showing build/run/log output.
class OutputPanel : public PanelBase {
public:
    explicit OutputPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme);

    ftxui::Component build();
    void focus() { focused_ = true; }
    void blur() { focused_ = false; }

    // Wire to EventBus for automatic output capture.
    void connect(events::EventBus& bus);

    // Manually append a line of output.
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
