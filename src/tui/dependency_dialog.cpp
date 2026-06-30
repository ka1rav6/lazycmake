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
    checked_.clear();
    for (const auto& dep : deps) {
        bool enabled = false;
        for (const auto& pd : project_.dependencies) {
            if (pd.name == dep->name && pd.enabled) {
                enabled = true;
                break;
            }
        }
        checked_.push_back(enabled);
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
            ftxui::text(" j/k: navigate   space: toggle   enter: apply   esc: cancel ")
            | ftxui::center | ftxui::dim);

        return ftxui::vbox(std::move(items)) | ftxui::border |
               ftxui::color(colorFromString(colors.foreground)) |
               ftxui::bgcolor(colorFromString(colors.background));
    }));

    return ftxui::CatchEvent(ftxui::Maybe(renderer, &visible_), [self](ftxui::Event e) {
        if (!self->visible_) return false;
        auto deps = self->registry_.all();
        if (deps.empty()) return false;

        if (e == ftxui::Event::Escape) {
            self->hide();
            return true;
        }
        if (e == ftxui::Event::Character('j') || e == ftxui::Event::ArrowDown) {
            self->selectedIndex_ = std::min(self->selectedIndex_ + 1, static_cast<int>(deps.size()) - 1);
            return true;
        }
        if (e == ftxui::Event::Character('k') || e == ftxui::Event::ArrowUp) {
            self->selectedIndex_ = std::max(0, self->selectedIndex_ - 1);
            return true;
        }
        if (e == ftxui::Event::Character(' ') &&
            self->selectedIndex_ >= 0 &&
            static_cast<size_t>(self->selectedIndex_) < self->checked_.size()) {
            self->checked_[self->selectedIndex_] = !self->checked_[self->selectedIndex_];
            return true;
        }
        if (e == ftxui::Event::Return) {
            // Apply checked dependencies to project.
            self->project_.dependencies.clear();
            for (size_t i = 0; i < deps.size() && i < self->checked_.size(); ++i) {
                if (self->checked_[i]) {
                    self->project_.dependencies.push_back(self->registry_.toSpec(*deps[i]));
                    self->project_.dependencies.back().enabled = true;
                }
            }
            self->hide();
            return true;
        }
        return false;
    });
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
