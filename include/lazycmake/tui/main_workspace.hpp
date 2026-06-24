#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// Three-panel workspace: Files | Targets | Output.
class MainWorkspace : public Screen {
public:
    explicit MainWorkspace(config::KeymapManager& keymap,
                           config::ThemeManager& theme);
    ~MainWorkspace() override = default;

    int activePanel() const { return activePanel_; }

private:
    ftxui::Component buildLayout();
    bool onEvent(ftxui::Event event);

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    int activePanel_ = 0;
    ftxui::Component layout_;
};

} // namespace lazycmake::tui
