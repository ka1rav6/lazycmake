#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/project.hpp"
#include "lazycmake/tui/panels/panel_base.hpp"

namespace lazycmake::tui {

// File panel — left-most panel showing project file tree.
// File panel — left-most panel showing project file tree.
// This panel provides a navigable tree view of project files, allowing users
// to browse and select files for building and editing operations.
// 
// Features:
//   - Displays hierarchical file structure of the current project
//   - Supports arrow key navigation (j/k, ↑/↓) to move selection
//   - Highlights focused item for better visibility
//   - Shows helpful tooltips when no project is loaded
//   - Compatible with the PanelBase interface for consistent TUI integration
class FilePanel : public PanelBase {
public:
    explicit FilePanel(config::KeymapManager& keymap,
                       config::ThemeManager& theme);

    // Explicit virtual destructor for proper cleanup of dynamically allocated panels
    virtual ~FilePanel() = default;
    
    ftxui::Component build() override;
    void focus() override { focused_ = true; }
    void blur() override { focused_ = false; }
    bool onEvent(ftxui::Event event) override;
    void show() override { visible_ = true; }
    void hide() override { visible_ = false; }
    void toggle() override { visible_ = !visible_; }
    bool isVisible() const override { return visible_; }

    // Point panel at a project root for real file listing.
    void setProject(const core::Project& project);

private:
    void refreshFileList();
    ftxui::Element render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    bool focused_ = false;
    bool visible_ = true;

    std::filesystem::path rootDir_;
    std::vector<std::filesystem::path> files_;
    int selectedIndex_ = 0;
};

} // namespace lazycmake::tui
