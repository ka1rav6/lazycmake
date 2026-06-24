#include "lazycmake/tui/application.hpp"

#include <spdlog/spdlog.h>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/cmake_gen/modern_cmake_generator.hpp"
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

ftxui::Component Application::buildRootComponent() {
    auto self = this;

    auto startup = std::make_unique<StartupScreen>(keymap_);
    startup->setOnAction([self](int idx) {
        self->onStartupAction(idx);
    });
    startupScreenPtr_ = startup.get();

    // Build a fullscreen renderer that wraps the startup menu with theme-aware decoration.
    auto startupRenderer = ftxui::Renderer(startup->component, [self] {
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
                   footer,
               }) |
               ftxui::bgcolor(colorFromString(colors.background)) |
               ftxui::color(colorFromString(colors.foreground));
    });

    startup->component = startupRenderer;
    screens_.push(std::move(startup));

    rootComponent_ = ftxui::Container::Vertical({
        screens_.current()->component,
    });

    return rootComponent_;
}

void Application::onStartupAction(int idx) {
    switch (idx) {
        case 0: { // New Project
            auto wizard = std::make_unique<WizardScreen>(keymap_);
            auto* wptr = wizard.get();
            wizard->setOnGenerate([this, wptr](core::Project project) {
                // Save project to disk.
                core::ProjectRepository repo;
                auto result = repo.save(project);
                if (result) {
                    spdlog::info("Project '{}' generated at {}",
                                 project.name, project.rootDir.string());
                } else {
                    spdlog::error("Failed to save project: {}",
                                  result.error().message);
                }
                // Navigate to main workspace.
                navigateToWorkspace(std::move(project));
            });
            wizard->setOnCancel([this] {
                navigateToStartup();
            });
            screens_.replace(std::move(wizard));
            // Rebuild root with new current screen.
            updateRootComponent();
            break;
        }
        case 4: { // Settings
            auto settings = std::make_unique<SettingsScreen>(keymap_, settings_);
            settings->component = settings->component | ftxui::CatchEvent([this](ftxui::Event e) {
                if (e == ftxui::Event::Escape || e == ftxui::Event::Character('q')) {
                    navigateToStartup();
                    return true;
                }
                return false;
            });
            screens_.push(std::move(settings));
            updateRootComponent();
            break;
        }
        case 5: // Quit
            screen_.ExitLoopClosure()();
            break;
    }
}

void Application::navigateToStartup() {
    while (screens_.size() > 1) {
        screens_.pop();
    }
    updateRootComponent();
}

void Application::navigateToWorkspace(core::Project project) {
    auto workspace = std::make_unique<MainWorkspace>(keymap_, theme_);
    workspace->setProject(project);
    workspace->setEventBus(eventBus_);
    while (screens_.size() > 1) {
        screens_.pop();
    }
    screens_.replace(std::move(workspace));
    updateRootComponent();
}

void Application::updateRootComponent() {
    auto* current = screens_.current();
    if (!current) return;

    rootComponent_ = ftxui::Container::Vertical({
        current->component,
    });

    screen_.PostEvent(ftxui::Event::Custom);
}

int Application::run(int /*argc*/, char** /*argv*/) {
    spdlog::set_level(spdlog::level::info);
    loadConfig();

    auto root = buildRootComponent();
    screen_.Loop(root);

    return 0;
}

} // namespace lazycmake::tui
