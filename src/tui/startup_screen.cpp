#include "lazycmake/tui/startup_screen.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

namespace {

const std::vector<std::string> kMenuItems = {
    "New Project",
    "Open Project",
    "Recent Projects",
    "Templates",
    "Settings",
    "Quit",
};

} // namespace

StartupScreen::StartupScreen(config::KeymapManager& keymap)
    : keymap_(keymap) {
    name = "startup";
    menu_ = buildMenu();
    component = ftxui::Container::Vertical({
        menu_,
    });
}

ftxui::Component StartupScreen::buildMenu() {
    auto menu = ftxui::Menu(&kMenuItems, &selectedIndex_);
    return menu;
}

} // namespace lazycmake::tui
