#pragma once

#include <functional>
#include <memory>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/project.hpp"
#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/tui/panels/file_panel.hpp"
#include "lazycmake/tui/panels/output_panel.hpp"
#include "lazycmake/tui/panels/panel_base.hpp"
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

    void setOnBuild(std::function<void()> cb);
    void setOnRun(std::function<void()> cb);
    void setOnDeps(std::function<void()> cb);
    void setOnHelp(std::function<void()> cb);
    void setOnExit(std::function<void()> cb);

    int activePanel() const { return activePanel_; }

private:
    void updatePanelFocus();
    PanelBase* activePanelImpl() const;

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    int activePanel_ = 0;
    ftxui::Component layout_;

    std::shared_ptr<FilePanel> filePanel_;
    std::shared_ptr<TargetPanel> targetPanel_;
    std::shared_ptr<OutputPanel> outputPanel_;

    std::function<void()> onBuild_;
    std::function<void()> onRun_;
    std::function<void()> onDeps_;
    std::function<void()> onHelp_;
    std::function<void()> onExit_;
};

} // namespace lazycmake::tui
