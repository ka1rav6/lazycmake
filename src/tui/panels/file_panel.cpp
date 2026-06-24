#include "lazycmake/tui/panels/file_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

FilePanel::FilePanel(config::KeymapManager& keymap,
                     config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    // Static dummy file list for Phase 5.
    files_ = {
        "src/main.cpp",
        "src/engine.cpp",
        "include/engine.hpp",
        "CMakeLists.txt",
        "README.md",
    };
}

ftxui::Component FilePanel::build() {
    auto self = this;
    auto renderer = ftxui::Renderer([self] {
        return self->render();
    });
    return renderer;
}

ftxui::Element FilePanel::render() {
    const auto& colors = theme_.activeColors();
    auto bg = focused_ ? colors.panelBackground : colors.listInactiveBackground;
    auto fg = focused_ ? colors.foreground : colors.listInactiveForeground;
    auto selBg = colors.listSelectionBackground;
    auto selFg = colors.listSelectionForeground;

    ftxui::Elements items;
    for (size_t i = 0; i < files_.size(); ++i) {
        auto prefix = (files_[i].find('/') != std::string::npos) ? "  " : "";
        auto text = ftxui::text(prefix + files_[i]);
        if (i == static_cast<size_t>(selectedIndex_) && focused_) {
            items.push_back(text | ftxui::bgcolor(colorFromString(selBg))
                                   | ftxui::color(colorFromString(selFg)));
        } else {
            items.push_back(text | ftxui::color(colorFromString(fg)));
        }
    }

    auto content = ftxui::vbox(std::move(items));
    auto title = focused_ ? ftxui::text(" Files ") | ftxui::bold
                          : ftxui::text(" Files ") | ftxui::dim;

    auto borderColor = focused_ ? colors.panelBorderFocused
                                : colors.panelBorderUnfocused;
    return ftxui::vbox({
               title,
               ftxui::separator(),
               content | ftxui::frame | ftxui::flex,
           }) |
           ftxui::border | ftxui::color(colorFromString(borderColor));
}

} // namespace lazycmake::tui
