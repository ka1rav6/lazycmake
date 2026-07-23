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

BuildOverlay::~BuildOverlay() {
    if (buildProgressId_) bus_.unsubscribe(buildProgressId_);
    if (buildFinishedId_) bus_.unsubscribe(buildFinishedId_);
    if (configFinishedId_) bus_.unsubscribe(configFinishedId_);
}

ftxui::Component BuildOverlay::build() {
    auto self = this;

    buildProgressId_ = bus_.subscribe<events::BuildProgressEvent>(
        [self](const events::BuildProgressEvent& e) {
            self->appendLog("[progress] " + e.stage + ": " + e.detail);
        });
    buildFinishedId_ = bus_.subscribe<events::BuildFinishedEvent>(
        [self](const events::BuildFinishedEvent& e) {
            self->setBuilding(false);
            if (e.success) {
                self->appendLog("[done] Build of '" + e.targetName
                                + "' succeeded (exit " + std::to_string(e.exitCode) + ")");
            } else {
                self->appendLog("[failed] Build of '" + e.targetName
                                + "' FAILED (exit " + std::to_string(e.exitCode) + ")");
            }
        });
    configFinishedId_ = bus_.subscribe<events::ConfigureFinishedEvent>(
        [self](const events::ConfigureFinishedEvent& e) {
            if (e.success) {
                self->appendLog("[configure] CMake configured successfully");
                if (!e.output.empty()) self->appendLog(e.output);
            } else {
                self->appendLog("[configure] CMake configure FAILED: " + e.errorMsg);
            }
        });

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
            : ftxui::text("Build: " + stageStr);

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

    return ftxui::CatchEvent(ftxui::Maybe(renderer, &visible_), [self](ftxui::Event e) {
        if (!self->visible_) return false;
        if (e == ftxui::Event::Escape) {
            self->hide();
            return true;
        }
        if (e == ftxui::Event::Character('c')) {
            self->buildManager_.cancel();
            return true;
        }
        if (e == ftxui::Event::Character('b') && !self->isBuilding_) {
            auto& mgr = self->buildManager_;
            if (!mgr.isRunning()) {
                if (mgr.currentStage() == build::BuildStage::Idle &&
                    !self->buildDir_.empty()) {
                    mgr.setConfigureParams(self->buildDir_, self->buildDir_ + "/build",
                                           self->generator_, self->buildType_);
                }
                self->setBuilding(true);
                mgr.build(self->target_);
            }
            return true;
        }
        return false;
    });
}

void BuildOverlay::show() {
    visible_ = true;
    logLines_.clear();
    isBuilding_ = false;
    logLines_.push_back("Build overlay opened. Press 'b' to start build.");
}

void BuildOverlay::hide() { visible_ = false; }
void BuildOverlay::toggle() { visible_ = !visible_; }

void BuildOverlay::setBuildDir(const std::string& dir) { buildDir_ = dir; }
void BuildOverlay::setTarget(const std::string& target) { target_ = target; }

void BuildOverlay::appendLog(const std::string& line) {
    logLines_.push_back(line);
}

void BuildOverlay::setBuilding(bool b) { isBuilding_ = b; }

} // namespace lazycmake::tui
