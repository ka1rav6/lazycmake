#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

#include "lazycmake/build/build_manager.hpp"
#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/events/event_bus.hpp"

namespace lazycmake::tui {

class BuildOverlay {
public:
    BuildOverlay(events::EventBus& bus,
                 config::KeymapManager& keymap,
                 config::ThemeManager& theme,
                 build::BuildManager& buildManager);
    ~BuildOverlay();

    ftxui::Component build();
    void show();
    void hide();
    bool isVisible() const { return visible_; }
    void toggle();

    void setBuildDir(const std::string& dir);
    void setTarget(const std::string& target);

public:
    void appendLog(const std::string& line);
    void setBuilding(bool b);

private:
    events::EventBus& bus_;
    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    build::BuildManager& buildManager_;

    bool visible_ = false;
    std::string buildDir_;
    std::string target_;
    std::vector<std::string> logLines_;
    bool isBuilding_ = false;

    events::EventBus::HandlerId buildProgressId_{0};
    events::EventBus::HandlerId buildFinishedId_{0};
    events::EventBus::HandlerId configFinishedId_{0};
};

} // namespace lazycmake::tui
