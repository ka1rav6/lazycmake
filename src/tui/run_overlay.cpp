#include "lazycmake/tui/run_overlay.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/color_helper.hpp"

namespace lazycmake::tui {

RunOverlay::RunOverlay(events::EventBus& bus,
                       config::KeymapManager& keymap,
                       config::ThemeManager& theme,
                       build::RunManager& runManager)
    : bus_(bus), keymap_(keymap), theme_(theme), runManager_(runManager) {}

RunOverlay::~RunOverlay() {
    if (runOutputId_) bus_.unsubscribe(runOutputId_);
    if (runFinishedId_) bus_.unsubscribe(runFinishedId_);
    if (runStartedId_) bus_.unsubscribe(runStartedId_);
}

ftxui::Component RunOverlay::build() {
    auto self = this;

    // Subscribe to run events.
    runStartedId_ = bus_.subscribe<events::RunStartedEvent>(
        [self](const events::RunStartedEvent& e) {
            self->logLines_.push_back("[start] Running " + e.executablePath);
        });
    runOutputId_ = bus_.subscribe<events::RunOutputEvent>(
        [self](const events::RunOutputEvent& e) {
            self->logLines_.push_back("[" + e.stream + "] " + e.line);
        });
    runFinishedId_ = bus_.subscribe<events::RunFinishedEvent>(
        [self](const events::RunFinishedEvent& e) {
            self->isRunning_ = false;
            if (e.success) {
                self->logLines_.push_back("[done] Process exited with code "
                                          + std::to_string(e.exitCode));
            } else {
                self->logLines_.push_back("[failed] Process failed with code "
                                          + std::to_string(e.exitCode));
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

        auto statusText = self->isRunning_
            ? ftxui::text("Running...")
            : ftxui::text("Ready");

        auto frame = ftxui::vbox({
            ftxui::text(" Run ") | ftxui::bold | ftxui::center,
            ftxui::separator(),
            statusText,
            ftxui::text(" Executable: " + self->executable_),
            log,
            ftxui::separator(),
            ftxui::text(" 'r': run   'k': kill   'esc': close ") | ftxui::center | ftxui::dim,
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
        if (e == ftxui::Event::Character('r') && !self->isRunning_) {
            self->logLines_.clear();
            self->runManager_.run(self->executable_, self->args_);
            return true;
        }
        if (e == ftxui::Event::Character('k') && self->isRunning_) {
            self->runManager_.kill();
            self->logLines_.push_back("Process killed by user.");
            self->isRunning_ = false;
            return true;
        }
        return false;
    });
}

void RunOverlay::show() {
    visible_ = true;
    logLines_.clear();
    isRunning_ = false;
    logLines_.push_back("Run overlay opened. Press 'r' to run.");
}

void RunOverlay::hide() { visible_ = false; }
void RunOverlay::toggle() { visible_ = !visible_; }

void RunOverlay::setExecutable(const std::string& exe) { executable_ = exe; }
void RunOverlay::setArgs(const std::vector<std::string>& args) { args_ = args; }

} // namespace lazycmake::tui
