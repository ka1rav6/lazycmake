#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/build/run_manager.hpp"
#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/events/event_bus.hpp"

namespace lazycmake::tui {

class RunOverlay {
public:
    RunOverlay(events::EventBus& bus,
               config::KeymapManager& keymap,
               config::ThemeManager& theme,
               build::RunManager& runManager);

    ftxui::Component build();
    void show();
    void hide();
    bool isVisible() const { return visible_; }
    void toggle();

    void setExecutable(const std::string& exe);
    void setArgs(const std::vector<std::string>& args);

private:
    ftxui::Component render();

    events::EventBus& bus_;
    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    build::RunManager& runManager_;

    bool visible_ = false;
    std::string executable_;
    std::vector<std::string> args_;
    std::vector<std::string> logLines_;
    bool isRunning_ = false;
    bool showArgsForm_ = false;

    events::EventBus::HandlerId runOutputId_{0};
    events::EventBus::HandlerId runFinishedId_{0};
    events::EventBus::HandlerId runStartedId_{0};
};

} // namespace lazycmake::tui
