#include "lazycmake/tui/panels/output_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

OutputPanel::OutputPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    // Placeholder welcome message for Phase 5.
    lines_ = {
        "Welcome to LazyCMake!",
        "",
        "This is the Output panel.",
        "Build and run output will appear here.",
        "",
        "Try pressing 'b' to build (Phase 6).",
    };
}

ftxui::Component OutputPanel::build() {
    auto self = this;
    auto renderer = ftxui::Renderer([self] {
        return self->render();
    });
    return renderer;
}

void OutputPanel::append(const std::string& line) {
    lines_.push_back(line);
    // Auto-scroll to bottom.
    if (static_cast<int>(lines_.size()) > scrollOffset_) {
        scrollOffset_ = static_cast<int>(lines_.size()) - 1;
    }
}

ftxui::Element OutputPanel::render() {
    const auto& colors = theme_.activeColors();
    auto fg = focused_ ? colors.foreground : colors.listInactiveForeground;

    ftxui::Elements items;
    for (const auto& line : lines_) {
        items.push_back(ftxui::text(line) | ftxui::color(colorFromString(fg)));
    }

    auto content = ftxui::vbox(std::move(items));
    auto title = focused_ ? ftxui::text(" Output ") | ftxui::bold
                          : ftxui::text(" Output ") | ftxui::dim;

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
