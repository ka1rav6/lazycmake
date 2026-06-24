#include "lazycmake/tui/main_workspace.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

MainWorkspace::MainWorkspace(config::KeymapManager& keymap,
                             config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    name = "main_workspace";

    filePanel_ = std::make_shared<FilePanel>(keymap, theme);
    targetPanel_ = std::make_shared<TargetPanel>(keymap, theme);
    outputPanel_ = std::make_shared<OutputPanel>(keymap, theme);

    filePanel_->focus();

    layout_ = ftxui::Container::Horizontal({
        filePanel_->build() | ftxui::flex,
        targetPanel_->build() | ftxui::flex,
        outputPanel_->build() | ftxui::flex,
    });

    component = layout_ | ftxui::border;
}

void MainWorkspace::setProject(const core::Project& project) {
    filePanel_->setProject(project);
    targetPanel_->setProject(project);
}

void MainWorkspace::setEventBus(events::EventBus& bus) {
    outputPanel_->connect(bus);
}

} // namespace lazycmake::tui
