#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/project.hpp"

namespace lazycmake::tui {

// File panel — left-most panel showing project file tree.
class FilePanel {
public:
    explicit FilePanel(config::KeymapManager& keymap,
                       config::ThemeManager& theme);

    ftxui::Component build();
    void focus() { focused_ = true; }
    void blur() { focused_ = false; }

    // Point panel at a project root for real file listing.
    void setProject(const core::Project& project);

private:
    void refreshFileList();
    bool onEvent(ftxui::Event event);
    ftxui::Element render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    bool focused_ = false;

    std::filesystem::path rootDir_;
    std::vector<std::filesystem::path> files_;
    int selectedIndex_ = 0;
};

} // namespace lazycmake::tui
