#include "lazycmake/tui/build_overlay.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

BuildOverlay::BuildOverlay(events::EventBus& bus,
                           config::KeymapManager& keymap,
                           config::ThemeManager& theme,
                           build::BuildManager& buildManager)
    : bus_(bus), keymap_(keymap), theme_(theme), buildManager_(buildManager) {}

ftxui::Component BuildOverlay::build() {
    auto self = this;
    auto renderer = ftxui::Renderer(std::function<ftxui::Element()>([self] {
        const auto& colors = self->theme_.activeColors();
        if (!self->visible_) return ftxui::emptyElement();

        ftxui::Elements logItems;
        for (const auto& line : self->logLines_) {
            logItems.push_back(ftxui::text(line));
        }

        auto log = ftxui::vbox(std::move(logItems)) | ftxui::frame | ftxui::flex;

        auto stageStr = build::toString(self->buildManager_.currentStage());
        auto statusText = self->isBuilding_
            ? ftxui::text("Building... [" + stageStr + "]")
            : ftxui::text("Build " + stageStr);

        auto frame = ftxui::vbox({
            ftxui::text(" Build ") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            statusText,
            ftxui::separator(),
            log,
            ftxui::separator(),
            ftxui::text(" 'b': build   'c': cancel   'esc': close ") | ftxui::center | ftxui::dim,
        });

        return frame | ftxui::border |
               ftxui::color(colorFromString(colors.foreground)) |
               ftxui::bgcolor(colorFromString(colors.background));
    }));

    return ftxui::Maybe(renderer, &visible_);
}

void BuildOverlay::show() {
    visible_ = true;
    logLines_.clear();
    isBuilding_ = false;
}

void BuildOverlay::hide() { visible_ = false; }
void BuildOverlay::toggle() { visible_ = !visible_; }

void BuildOverlay::setBuildDir(const std::string& dir) { buildDir_ = dir; }
void BuildOverlay::setTarget(const std::string& target) { target_ = target; }

} // namespace lazycmake::tui
