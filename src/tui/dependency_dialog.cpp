#include "lazycmake/tui/dependency_dialog.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

DependencyDialog::DependencyDialog(config::KeymapManager& keymap,
                                   config::ThemeManager& theme,
                                   core::DependencyRegistry& registry,
                                   core::Project& project)
    : keymap_(keymap), theme_(theme), registry_(registry), project_(project) {}

ftxui::Component DependencyDialog::build() {
    auto deps = registry_.all();
    checked_.resize(deps.size(), false);

    for (const auto& dep : project_.dependencies) {
        for (size_t i = 0; i < deps.size(); ++i) {
            if (deps[i]->name == dep.name) {
                checked_[i] = dep.enabled;
                break;
            }
        }
    }

    auto self = this;
    auto renderer = ftxui::Renderer(std::function<ftxui::Element()>([self] {
        const auto& colors = self->theme_.activeColors();
        if (!self->visible_) return ftxui::emptyElement();

        auto deps = self->registry_.all();
        ftxui::Elements items;

        items.push_back(ftxui::text("Available Dependencies") | ftxui::bold | ftxui::center);
        items.push_back(ftxui::separator());

        for (size_t i = 0; i < deps.size(); ++i) {
            auto prefix = (i < self->checked_.size() && self->checked_[i])
                ? ftxui::text("[x] ") : ftxui::text("[ ] ");
            auto name = ftxui::text(deps[i]->name);
            auto desc = ftxui::text("  " + deps[i]->description) | ftxui::dim;

            auto line = ftxui::hbox({prefix, name, desc});

            if (i == static_cast<size_t>(self->selectedIndex_)) {
                line = line | ftxui::bgcolor(colorFromString(colors.listSelectionBackground))
                            | ftxui::color(colorFromString(colors.listSelectionForeground));
            }

            items.push_back(line);
        }

        items.push_back(ftxui::separator());
        items.push_back(
            ftxui::text(" j/k: navigate   space: toggle   enter: confirm   esc: cancel ")
            | ftxui::center | ftxui::dim);

        return ftxui::vbox(std::move(items)) | ftxui::border |
               ftxui::color(colorFromString(colors.foreground)) |
               ftxui::bgcolor(colorFromString(colors.background));
    }));

    return ftxui::Maybe(renderer, &visible_);
}

void DependencyDialog::show() { visible_ = true; }
void DependencyDialog::hide() { visible_ = false; }
void DependencyDialog::toggle() { visible_ = !visible_; }

std::vector<std::string> DependencyDialog::selectedDeps() const {
    auto deps = registry_.all();
    std::vector<std::string> selected;
    for (size_t i = 0; i < checked_.size(); ++i) {
        if (checked_[i]) selected.push_back(deps[i]->name);
    }
    return selected;
}

} // namespace lazycmake::tui
