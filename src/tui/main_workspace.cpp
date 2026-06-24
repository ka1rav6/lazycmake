#include "lazycmake/tui/main_workspace.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/panels/file_panel.hpp"
#include "lazycmake/tui/panels/target_panel.hpp"
#include "lazycmake/tui/panels/output_panel.hpp"

namespace lazycmake::tui {

MainWorkspace::MainWorkspace(config::KeymapManager& keymap,
                             config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    name = "main_workspace";

    auto filePanel = std::make_shared<FilePanel>(keymap, theme);
    auto targetPanel = std::make_shared<TargetPanel>(keymap, theme);
    auto outputPanel = std::make_shared<OutputPanel>(keymap, theme);

    filePanel->focus();  // Start with file panel focused.

    layout_ = ftxui::Container::Horizontal({
        filePanel->build() | ftxui::flex,
        targetPanel->build() | ftxui::flex,
        outputPanel->build() | ftxui::flex,
    });

    component = layout_ | ftxui::border;
}

} // namespace lazycmake::tui
