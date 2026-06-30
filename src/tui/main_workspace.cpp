#include "lazycmake/tui/main_workspace.hpp"

#include <spdlog/spdlog.h>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/tui/build_overlay.hpp"
#include "lazycmake/tui/run_overlay.hpp"
#include "lazycmake/tui/dependency_dialog.hpp"
#include "lazycmake/tui/conflict_dialog.hpp"
#include "lazycmake/tui/help_overlay.hpp"

namespace lazycmake::tui {

MainWorkspace::MainWorkspace(config::KeymapManager& keymap,
                             config::ThemeManager& theme)
    : keymap_(keymap), theme_(theme) {
    name = "main_workspace";

    filePanel_ = std::make_shared<FilePanel>(keymap, theme);
    targetPanel_ = std::make_shared<TargetPanel>(keymap, theme);
    outputPanel_ = std::make_shared<OutputPanel>(keymap, theme);

    filePanel_->focus();

    auto fp = filePanel_->build();
    auto tp = targetPanel_->build();
    auto op = outputPanel_->build();

    auto panels = ftxui::Container::Horizontal({
        fp | ftxui::flex,
        tp | ftxui::flex,
        op | ftxui::flex,
    });

    layout_ = panels | ftxui::border;

    component = ftxui::CatchEvent(layout_, [this](ftxui::Event e) {
        if (e == ftxui::Event::Character('q')) {
            spdlog::debug("Workspace: 'q' pressed, going back");
            if (onExit_) onExit_();
            return true;
        }
        if (e == ftxui::Event::Tab) {
            activePanel_ = (activePanel_ + 1) % 3;
            spdlog::debug("Workspace: Tab -> panel {}", activePanel_);
            updatePanelFocus();
            return true;
        }
        if (e == ftxui::Event::TabReverse) {
            activePanel_ = (activePanel_ + 2) % 3;
            spdlog::debug("Workspace: Shift+Tab -> panel {}", activePanel_);
            updatePanelFocus();
            return true;
        }
        if (e == ftxui::Event::Character('j') || e == ftxui::Event::ArrowDown) {
            auto* panel = activePanelImpl();
            if (panel) panel->onEvent(e);
            return true;
        }
        if (e == ftxui::Event::Character('k') || e == ftxui::Event::ArrowUp) {
            auto* panel = activePanelImpl();
            if (panel) panel->onEvent(e);
            return true;
        }
        if (e == ftxui::Event::Character('b')) {
            spdlog::info("Workspace: 'b' -> build overlay");
            if (onBuild_) onBuild_();
            return true;
        }
        if (e == ftxui::Event::Character('r')) {
            spdlog::info("Workspace: 'r' -> run overlay");
            if (onRun_) onRun_();
            return true;
        }
        if (e == ftxui::Event::Character('d')) {
            spdlog::info("Workspace: 'd' -> dependency dialog");
            if (onDeps_) onDeps_();
            return true;
        }
        if (e == ftxui::Event::Character('h')) {
            spdlog::info("Workspace: 'h' -> help overlay");
            if (onHelp_) onHelp_();
            return true;
        }
        return false;
    });
}

void MainWorkspace::updatePanelFocus() {
    filePanel_->blur();
    targetPanel_->blur();
    outputPanel_->blur();

    switch (activePanel_) {
        case 0: filePanel_->focus(); break;
        case 1: targetPanel_->focus(); break;
        case 2: outputPanel_->focus(); break;
    }
}

PanelBase* MainWorkspace::activePanelImpl() const {
    switch (activePanel_) {
        case 0: return filePanel_.get();
        case 1: return targetPanel_.get();
        case 2: return outputPanel_.get();
    }
    return nullptr;
}

void MainWorkspace::setProject(const core::Project& project) {
    filePanel_->setProject(project);
    targetPanel_->setProject(project);
}

void MainWorkspace::setEventBus(events::EventBus& bus) {
    outputPanel_->connect(bus);
}

void MainWorkspace::setOnBuild(std::function<void()> cb) {
    onBuild_ = std::move(cb);
}

void MainWorkspace::setOnRun(std::function<void()> cb) {
    onRun_ = std::move(cb);
}

void MainWorkspace::setOnDeps(std::function<void()> cb) {
    onDeps_ = std::move(cb);
}

void MainWorkspace::setOnHelp(std::function<void()> cb) {
    onHelp_ = std::move(cb);
}

void MainWorkspace::setOnExit(std::function<void()> cb) {
    onExit_ = std::move(cb);
}

} // namespace lazycmake::tui
