#include "lazycmake/tui/application.hpp"

#include <spdlog/spdlog.h>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/cmake_gen/modern_cmake_generator.hpp"
#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/core/project_repository.hpp"
#include "lazycmake/tui/color_helper.hpp"
#include "lazycmake/tui/help_overlay.hpp"
#include "lazycmake/tui/main_workspace.hpp"
#include "lazycmake/tui/settings_screen.hpp"
#include "lazycmake/tui/startup_screen.hpp"
#include "lazycmake/tui/wizard_screen.hpp"

namespace lazycmake::tui {

Application::Application()
    : screen_(ftxui::ScreenInteractive::Fullscreen()) {}

Application::~Application() = default;

void Application::loadConfig() {
    settings_.loadAll();
    keymap_.loadAll();
    theme_.loadAll();
    spdlog::info("Configuration loaded");
}

ftxui::Component Application::makeStartupScreen() {
    auto self = this;
    auto screen = std::make_unique<StartupScreen>(keymap_);
    screen->setOnAction([self](int idx) {
        self->onStartupAction(idx);
    });

    // Compose title | menu | footer as a vertical stack,
    // then wrap with theme-aware backgrounds via a Renderer.
    auto menuComp = screen->component;

    // Render title + menu + footer as a single composite element.
    // Events are forwarded to menuComp (the CatchEvent-wrapped Menu), which
    // handles j/k for item navigation and Enter for selection. We do NOT use
    // Container::Vertical here because its focus-cycling would intercept j/k.
    auto styled = ftxui::Renderer(menuComp, std::function<ftxui::Element()>([self, menuComp] {
        const auto& colors = self->theme_.activeColors();
        auto title = ftxui::vbox({
            ftxui::text("LazyCMake") | ftxui::bold | ftxui::center,
            ftxui::text("lazygit, but for CMake") | ftxui::center | ftxui::dim,
            ftxui::separator(),
            ftxui::text(""),
        });
        auto footer = ftxui::vbox({
            ftxui::text(""),
            ftxui::separator(),
            ftxui::text("Navigate: j/k   Select: enter   Quit: q   Help: h")
                | ftxui::center | ftxui::dim,
        });
        return ftxui::vbox({
                   title | ftxui::center,
                   ftxui::separator(),
                   menuComp->Render(),
                   ftxui::separator(),
                   footer,
               }) |
               ftxui::bgcolor(colorFromString(colors.background)) |
               ftxui::color(colorFromString(colors.foreground));
    }));

    return styled;
}

ftxui::Component Application::makeWizardScreen() {
    auto self = this;
    auto screen = std::make_unique<WizardScreen>(keymap_);
    screen->setOnGenerate([self](core::Project project) {
        self->onGenerateProject(std::move(project));
    });
    screen->setOnCancel([self] {
        self->navigateToStartup();
    });
    return screen->component;
}

ftxui::Component Application::makeSettingsScreen() {
    auto self = this;
    auto screen = std::make_unique<SettingsScreen>(keymap_, settings_);
    auto wrapped = screen->component | ftxui::CatchEvent([self](ftxui::Event e) {
        if (e == ftxui::Event::Escape || e == ftxui::Event::Character('q')) {
            self->navigateToStartup();
            return true;
        }
        return false;
    });
    return wrapped;
}

ftxui::Component Application::makeWorkspaceScreen(core::Project project) {
    auto screen = std::make_unique<MainWorkspace>(keymap_, theme_);
    screen->setProject(project);
    screen->setEventBus(eventBus_);
    return screen->component;
}

void Application::onStartupAction(int idx) {
    switch (idx) {
        case 0: navigateToWizard(); break;
        case 4: navigateToSettings(); break;
        case 5: screen_.ExitLoopClosure()(); break;
    }
}

void Application::navigateToStartup() {
    currentScreen_ = makeStartupScreen();
}

void Application::navigateToWizard() {
    currentScreen_ = makeWizardScreen();
}

void Application::navigateToSettings() {
    currentScreen_ = makeSettingsScreen();
}

void Application::navigateToWorkspace(core::Project project) {
    currentScreen_ = makeWorkspaceScreen(std::move(project));
}

void Application::onGenerateProject(core::Project project) {
    core::ProjectRepository repo;
    auto result = repo.save(project);
    if (result) {
        spdlog::info("Project '{}' generated at {}",
                     project.name, project.rootDir.string());
    } else {
        spdlog::error("Failed to save project: {}",
                      result.error().message);
    }
    navigateToWorkspace(std::move(project));
}

int Application::run(int /*argc*/, char** /*argv*/) {
    spdlog::set_level(spdlog::level::info);
    loadConfig();

    // Create the ScreenHolder that delegates to currentScreen_.
    screenHolder_ = std::make_shared<ScreenHolder>(&currentScreen_);

    // Set initial screen.
    currentScreen_ = makeStartupScreen();

    screen_.Loop(screenHolder_);
    return 0;
}

} // namespace lazycmake::tui
