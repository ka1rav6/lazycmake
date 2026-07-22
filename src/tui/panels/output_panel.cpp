#include "lazycmake/tui/panels/output_panel.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

OutputPanel::OutputPanel(config::KeymapManager& keymap,
                         config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    lines_ = {
        "Welcome to LazyCMake!",
        "",
        "This is the Output panel.",
        "Build and run output will appear here.",
        "",
        "Try pressing 'b' to build.",
    };
}

ftxui::Component OutputPanel::build() {
    auto self = this;
    auto renderer = ftxui::Renderer(std::function<ftxui::Element()>([self] {
        return self->render();
    }));

    auto wrapped = renderer | ftxui::CatchEvent([self](ftxui::Event event) {
        return self->onEvent(event);
    });

    return wrapped;
}

void OutputPanel::connect(events::EventBus& bus) {
    bus_ = &bus;
    runOutputId_ = bus.subscribe<events::RunOutputEvent>([this](const events::RunOutputEvent& e) {
        append("[" + e.stream + "] " + e.line);
    });
    buildProgressId_ = bus.subscribe<events::BuildProgressEvent>([this](const events::BuildProgressEvent& e) {
        append("[build] " + e.stage + ": " + e.detail);
    });
    buildFinishedId_ = bus.subscribe<events::BuildFinishedEvent>([this](const events::BuildFinishedEvent& e) {
        if (e.success) {
            append("[build] Finished target '" + e.targetName + "' (exit " + std::to_string(e.exitCode) + ")");
        } else {
            append("[build] FAILED target '" + e.targetName + "' (exit " + std::to_string(e.exitCode) + ")");
        }
        if (!e.output.empty()) {
            append(e.output);
        }
    });
    runFinishedId_ = bus.subscribe<events::RunFinishedEvent>([this](const events::RunFinishedEvent& e) {
        if (e.success) {
            append("[run] Process exited with code " + std::to_string(e.exitCode));
        } else {
            append("[run] Process failed with code " + std::to_string(e.exitCode));
        }
    });
}

void OutputPanel::append(const std::string& line) {
    lines_.push_back(line);
    if (static_cast<int>(lines_.size()) > scrollOffset_) {
        scrollOffset_ = static_cast<int>(lines_.size()) - 1;
    }
}

bool OutputPanel::onEvent(ftxui::Event event) {
    if (lines_.empty()) return false;
    if (event == ftxui::Event::PageUp) {
        scrollOffset_ = std::max(0, scrollOffset_ - 10);
        return true;
    }
    if (event == ftxui::Event::PageDown) {
        scrollOffset_ = std::min(static_cast<int>(lines_.size()) - 1, scrollOffset_ + 10);
        return true;
    }
    return false;
}

ftxui::Element OutputPanel::render() {
    const auto& colors = theme_.activeColors();
    auto fg = focused_ ? colors.foreground : colors.listInactiveForeground;

    ftxui::Elements items;
    int start = std::max(0, scrollOffset_ - 50);
    int end = std::min(static_cast<int>(lines_.size()), scrollOffset_ + 1);
    for (int i = start; i < end; ++i) {
        items.push_back(ftxui::text(lines_[i]) | ftxui::color(colorFromString(fg)));
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
