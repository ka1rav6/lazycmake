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
    component = menu_;
}

ftxui::Component StartupScreen::buildMenu() {
    auto menu = ftxui::Menu(&kMenuItems, &selectedIndex_);

    auto wrapped = menu | ftxui::CatchEvent([this](ftxui::Event event) {
        if (event == ftxui::Event::Return) {
            if (onAction_) onAction_(selectedIndex_);
            return true;
        }
        return false;
    });

    return wrapped;
}

} // namespace lazycmake::tui
