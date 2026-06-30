#include "lazycmake/tui/conflict_dialog.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

ConflictDialog::ConflictDialog(config::KeymapManager& keymap,
                               config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {}

ftxui::Component ConflictDialog::build() {
    auto self = this;
    auto renderer = ftxui::Renderer(std::function<ftxui::Element()>([self] {
        const auto& colors = self->theme_.activeColors();
        if (!self->visible_) return ftxui::emptyElement();

        auto choices = ftxui::vbox({
            ftxui::text("  [1] View Diff") | ftxui::color(colorFromString(colors.info)),
            ftxui::text("  [2] Overwrite (keep my changes in a .bak file)") | ftxui::color(colorFromString(colors.warning)),
            ftxui::text("  [3] Keep Mine & Stop Tracking This File") | ftxui::color(colorFromString(colors.error)),
        });

        auto frame = ftxui::vbox({
            ftxui::text(" File Conflict Detected ") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            ftxui::text(""),
            ftxui::text(" " + self->filePath_ + " was edited outside LazyCMake."),
            ftxui::text(""),
            choices,
            ftxui::text(""),
            ftxui::separator(),
            ftxui::text(" Choose: 1/2/3   esc: cancel ") | ftxui::center | ftxui::dim,
        });

        return frame | ftxui::border |
               ftxui::color(colorFromString(colors.foreground)) |
               ftxui::bgcolor(colorFromString(colors.background));
    }));

    return ftxui::CatchEvent(ftxui::Maybe(renderer, &visible_), [self](ftxui::Event e) {
        if (!self->visible_) return false;
        if (e == ftxui::Event::Escape) {
            self->hide();
            return true;
        }
        if (e == ftxui::Event::Character('1')) {
            self->chosenAction_ = core::GeneratedFileLock::ConflictAction::ViewDiff;
            self->onConflictResolved_();
            self->hide();
            return true;
        }
        if (e == ftxui::Event::Character('2')) {
            self->chosenAction_ = core::GeneratedFileLock::ConflictAction::Overwrite;
            self->onConflictResolved_();
            self->hide();
            return true;
        }
        if (e == ftxui::Event::Character('3')) {
            self->chosenAction_ = core::GeneratedFileLock::ConflictAction::KeepMine;
            self->onConflictResolved_();
            self->hide();
            return true;
        }
        return false;
    });
}

void ConflictDialog::show(const std::string& filePath,
                          core::GeneratedFileLock::ConflictResult result) {
    visible_ = true;
    filePath_ = filePath;
    result_ = result;
    chosenAction_ = core::GeneratedFileLock::ConflictAction::ViewDiff;
}

void ConflictDialog::hide() { visible_ = false; }

void ConflictDialog::setOnResolved(std::function<void()> cb) {
    onConflictResolved_ = std::move(cb);
}

} // namespace lazycmake::tui
