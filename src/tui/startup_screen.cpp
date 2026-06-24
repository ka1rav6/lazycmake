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

    auto self = this;
    auto wrapped = menu | ftxui::CatchEvent([self](ftxui::Event event) {
        // j/k navigation
        if (event == ftxui::Event::Character('j')) {
            self->selectedIndex_ = std::min(self->selectedIndex_ + 1,
                                            static_cast<int>(kMenuItems.size()) - 1);
            return true;
        }
        if (event == ftxui::Event::Character('k')) {
            self->selectedIndex_ = std::max(self->selectedIndex_ - 1, 0);
            return true;
        }
        // Enter — select current item
        if (event == ftxui::Event::Return) {
            if (self->onAction_) self->onAction_(self->selectedIndex_);
            return true;
        }
        // ArrowUp/ArrowDown handled natively by ftxui::Menu — let them pass.
        return false;
    });

    return wrapped;
}

} // namespace lazycmake::tui
