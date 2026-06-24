#include "lazycmake/tui/application.hpp"

#include <spdlog/spdlog.h>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

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
    auto startup = std::make_unique<StartupScreen>(keymap_);

    // Build a fullscreen renderer that wraps the startup menu with theme-aware decoration.
    auto startupRenderer = ftxui::Renderer(startup->component, [this] {
        const auto& colors = theme_.activeColors();

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

    return ftxui::Container::Vertical({
        screens_.current()->component,
    });
}

int Application::run(int /*argc*/, char** /*argv*/) {
    spdlog::set_level(spdlog::level::info);
    loadConfig();

    auto root = buildRootComponent();
    screen_.Loop(root);

    return 0;
}

} // namespace lazycmake::tui
