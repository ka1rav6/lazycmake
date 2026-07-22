#include "lazycmake/tui/panels/target_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/core/types.hpp"
#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

TargetPanel::TargetPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    targets_ = {};
}

ftxui::Component TargetPanel::build() {
    auto self = this;
    auto renderer = ftxui::Renderer(std::function<ftxui::Element()>([self] {
        return self->render();
    }));

    auto wrapped = renderer | ftxui::CatchEvent([self](ftxui::Event event) {
        return self->onEvent(event);
    });

    return wrapped;
}

void TargetPanel::setProject(const core::Project& project) {
    targets_.clear();
    for (const auto& target : project.targets) {
        targets_.push_back(target.name + " (" + core::toString(target.kind) + ")");
    }
    if (targets_.empty()) {
        targets_.push_back("(no targets)");
    }
    if (selectedIndex_ >= static_cast<int>(targets_.size())) {
        selectedIndex_ = static_cast<int>(targets_.size()) - 1;
    }
}

bool TargetPanel::onEvent(ftxui::Event event) {
    if (!focused_ || targets_.empty()) return false;

    if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('j')) {
        selectedIndex_ = std::min(selectedIndex_ + 1, static_cast<int>(targets_.size()) - 1);
        return true;
    }
    if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('k')) {
        selectedIndex_ = std::max(0, selectedIndex_ - 1);
        return true;
    }

    return false;
}

ftxui::Element TargetPanel::render() {
    const auto& colors = theme_.activeColors();
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
