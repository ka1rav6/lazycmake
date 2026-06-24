#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/config/theme_manager.hpp"
#include "lazycmake/core/dependency_registry.hpp"
#include "lazycmake/core/project.hpp"

namespace lazycmake::tui {

class DependencyDialog {
public:
    DependencyDialog(config::KeymapManager& keymap,
                     config::ThemeManager& theme,
                     core::DependencyRegistry& registry,
                     core::Project& project);

    ftxui::Component build();
    void show();
    void hide();
    bool isVisible() const { return visible_; }
    void toggle();

    std::vector<std::string> selectedDeps() const;

private:
    ftxui::Component render();

    config::KeymapManager& keymap_;
    config::ThemeManager& theme_;
    core::DependencyRegistry& registry_;
    core::Project& project_;

    bool visible_ = false;
    int selectedIndex_ = 0;
    std::vector<bool> checked_;
};

} // namespace lazycmake::tui
