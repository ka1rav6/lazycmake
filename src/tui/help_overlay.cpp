#include "lazycmake/tui/help_overlay.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

HelpOverlay::HelpOverlay(config::KeymapManager& keymap)
    : keymap_(keymap) {}

ftxui::Component HelpOverlay::build() {
    overlay_ = ftxui::Renderer([this] {
        if (!visible_) return ftxui::emptyElement();

        auto contexts = keymap_.contexts();
        ftxui::Elements columns;

        for (const auto& ctx : contexts) {
            auto bindings = keymap_.getBindings(ctx);
            ftxui::Elements ctxLines;
            ctxLines.push_back(ftxui::text("[" + ctx + "]") | ftxui::bold);
            for (const auto& b : bindings) {
                ctxLines.push_back(
                    ftxui::hbox({
                        ftxui::text("  " + b.key) | ftxui::color(ftxui::Color::Yellow),
                        ftxui::text("  \u2192  " + b.action),
                    })
                );
            }
            ctxLines.push_back(ftxui::text(""));
            columns.push_back(ftxui::vbox(std::move(ctxLines)));
        }

        auto content = ftxui::hbox(columns);
        auto frame = ftxui::vbox({
            ftxui::text(" Keybindings ") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            content,
            ftxui::separator(),
            ftxui::text(" Press 'h' or 'esc' to close this help.") | ftxui::center,
        });

        return frame | ftxui::border | ftxui::center;
    });

    return overlay_;
}

void HelpOverlay::show() { visible_ = true; }
void HelpOverlay::hide() { visible_ = false; }
void HelpOverlay::toggle() { visible_ = !visible_; }

} // namespace lazycmake::tui
