#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/project.hpp"
#include "lazycmake/tui/panels/panel_base.hpp"

namespace lazycmake::tui {

// Target panel — center panel showing project targets.
// This panel displays all build targets (executables, libraries, tests) from the
// current project. It allows users to view target details and select targets
// for building.
// 
// Features:
//   - Shows target names with their type in parentheses (e.g., "target_name (Static)")
//   - Supports arrow key navigation to move selection between targets
//   - Highlights selected target for easy identification
//   - Shows placeholder message when no project is loaded
//   - Integrates seamlessly with the PanelBase interface for consistent TUI behavior
// 
// Use case: Users can browse available targets and select one to build
class TargetPanel : public PanelBase {
public:
    explicit TargetPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme);

    virtual ~TargetPanel() = default;
    
    ftxui::Component build() override;
    void focus() override { focused_ = true; }
    void blur() override { focused_ = false; }
    bool onEvent(ftxui::Event event) override;
    void show() override { visible_ = true; }
    void hide() override { visible_ = false; }
    void toggle() override { visible_ = !visible_; }
    bool isVisible() const override { return visible_; }

    // Point panel at a project to show its targets.
    void setProject(const core::Project& project);

private:
    ftxui::Element render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    bool focused_ = false;
    bool visible_ = true;

    std::vector<std::string> targets_;
    int selectedIndex_ = 0;
};

} // namespace lazycmake::tui
