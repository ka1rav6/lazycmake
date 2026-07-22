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
// Output panel — right-most panel showing build/run/log output.
// This panel provides real-time output from build processes, run commands,
// and error reporting. It streams output as it becomes available and
// allows users to scroll through logs.
// 
// Features:
//   - Streams real-time output from build and run operations
//   - Displays colored status indicators ([build], [run], [done], [failed])
//   - Auto-scrolls to show latest output
//   - Supports keyboard scrolling (PageUp/PageDown) through output
//   - Integrates with EventBus for automatic output capture
// 
// Use case: View build logs, command output, and diagnostic information
// Use this panel to monitor project build and run operations in real-time.
class OutputPanel : public PanelBase {
public:
    explicit OutputPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme);

    virtual ~OutputPanel() = default;

    ftxui::Component build() override;
    void focus() override { focused_ = true; }
    void blur() override { focused_ = false; }
    bool onEvent(ftxui::Event event) override;
    void show() override { visible_ = true; }
    void hide() override { visible_ = false; }
    void toggle() override { visible_ = !visible_; }
    bool isVisible() const override { return visible_; }

    // Wire to EventBus for automatic output capture.
    void connect(events::EventBus& bus);

    // Manually append a line of output.
    void append(const std::string& line);

private:
    ftxui::Element render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    bool focused_ = false;
    bool visible_ = true;

    // In-memory log buffer.
    std::vector<std::string> lines_;
    int scrollOffset_ = 0;

    // EventBus subscription handles for cleanup.
    events::EventBus* bus_ = nullptr;
    events::EventBus::HandlerId runOutputId_{0};
    events::EventBus::HandlerId buildProgressId_{0};
    events::EventBus::HandlerId buildFinishedId_{0};
    events::EventBus::HandlerId runFinishedId_{0};
};

} // namespace lazycmake::tui
