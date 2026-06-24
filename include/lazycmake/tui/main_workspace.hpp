#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/project.hpp"
#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/tui/panels/file_panel.hpp"
#include "lazycmake/tui/panels/output_panel.hpp"
#include "lazycmake/tui/panels/target_panel.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// Three-panel workspace: Files | Targets | Output.
class MainWorkspace : public Screen {
public:
    explicit MainWorkspace(config::KeymapManager& keymap,
                           config::ThemeManager& theme);
    ~MainWorkspace() override = default;

    void setProject(const core::Project& project);
    void setEventBus(events::EventBus& bus);

    int activePanel() const { return activePanel_; }

private:
    ftxui::Component buildLayout();
    bool onEvent(ftxui::Event event);

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    int activePanel_ = 0;
    ftxui::Component layout_;

    std::shared_ptr<FilePanel> filePanel_;
    std::shared_ptr<TargetPanel> targetPanel_;
    std::shared_ptr<OutputPanel> outputPanel_;
};

} // namespace lazycmake::tui
