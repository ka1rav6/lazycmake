#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/project.hpp"

namespace lazycmake::tui {

// Target panel — center panel showing project targets.
class TargetPanel {
public:
    explicit TargetPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme);

    ftxui::Component build();
    void focus() { focused_ = true; }
    void blur() { focused_ = false; }

    // Point panel at a project to show its targets.
    void setProject(const core::Project& project);

private:
    bool onEvent(ftxui::Event event);
    ftxui::Element render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    bool focused_ = false;

    std::vector<std::string> targets_;
    int selectedIndex_ = 0;
};

} // namespace lazycmake::tui
