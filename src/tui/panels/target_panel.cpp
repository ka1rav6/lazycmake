#include "lazycmake/tui/panels/target_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

TargetPanel::TargetPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    // Static dummy target list for Phase 5.
    targets_ = {
        "my_game (Executable)",
        "engine (StaticLibrary)",
        "external::fmt (HeaderOnly)",
        "external::spdlog (HeaderOnly)",
    };
}

ftxui::Component TargetPanel::build() {
    auto self = this;
    auto renderer = ftxui::Renderer([self] {
        return self->render();
    });
    return renderer;
}

ftxui::Element TargetPanel::render() {
    const auto& colors = theme_.activeColors();
    auto bg = focused_ ? colors.panelBackground : colors.listInactiveBackground;
    auto fg = focused_ ? colors.foreground : colors.listInactiveForeground;
    auto selBg = colors.listSelectionBackground;
    auto selFg = colors.listSelectionForeground;

    ftxui::Elements items;
    for (size_t i = 0; i < targets_.size(); ++i) {
        auto text = ftxui::text(targets_[i]);

        if (i == static_cast<size_t>(selectedIndex_) && focused_) {
            items.push_back(text | ftxui::bgcolor(colorFromString(selBg))
                                   | ftxui::color(colorFromString(selFg)));
        } else {
            items.push_back(text | ftxui::color(colorFromString(fg)));
        }
    }

    auto content = ftxui::vbox(std::move(items));
    auto title = focused_ ? ftxui::text(" Targets ") | ftxui::bold
                          : ftxui::text(" Targets ") | ftxui::dim;

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
