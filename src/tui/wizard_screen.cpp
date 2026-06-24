#include "lazycmake/tui/wizard_screen.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

namespace {

const std::vector<std::string> kStepTitles = {
    "Step 1: Name & Language",
    "Step 2: Project Type",
    "Step 3: C++ Standard",
    "Step 4: Build System",
    "Step 5: Dependencies",
};

} // namespace

WizardScreen::WizardScreen(config::KeymapManager& keymap)
    : keymap_(keymap) {
    name = "wizard";

    // Build each step as a separate component, stacked.
    std::vector<ftxui::Component> steps;
    for (int i = 0; i < 5; ++i) {
        steps.push_back(buildStep(i));
    }

    auto nav = buildNavigation();

    container_ = ftxui::Container::Vertical({
        ftxui::Container::Tab(steps, &currentStep_),
        nav,
    });

    component = container_;
}

ftxui::Component WizardScreen::buildStep(int step) {
    return ftxui::Renderer([step] {
        switch (step) {
            case 0:
                return ftxui::vbox({
                    ftxui::text("Project Name:"),
                    ftxui::text("  [input field - Phase 6]"),
                    ftxui::text("Language: C / C++ / Both"),
                    ftxui::text("  [radio buttons - Phase 6]"),
                });
            case 1:
                return ftxui::vbox({
                    ftxui::text("Target Type:"),
                    ftxui::text("  Executable"),
                    ftxui::text("  Static Library"),
                    ftxui::text("  Shared Library"),
                });
            case 2:
                return ftxui::vbox({
                    ftxui::text("C++ Standard:"),
                    ftxui::text("  C++17"),
                    ftxui::text("  C++20  [recommended]"),
                    ftxui::text("  C++23"),
                });
            case 3:
                return ftxui::vbox({
                    ftxui::text("Build System:"),
                    ftxui::text("  Ninja  [recommended]"),
                    ftxui::text("  Unix Makefiles"),
                });
            case 4:
                return ftxui::vbox({
                    ftxui::text("Dependencies (check to add):"),
                    ftxui::text("  [ ] fmt"),
                    ftxui::text("  [ ] spdlog"),
                    ftxui::text("  [ ] nlohmann_json"),
                    ftxui::text("  [ ] Catch2"),
                    ftxui::text("  [ ] Boost.Asio"),
                });
            default:
                return ftxui::text("Unknown step");
        }
    });
}

ftxui::Component WizardScreen::buildNavigation() {
    return ftxui::Container::Horizontal({
        ftxui::Button(" < Back ", [this] {
            if (currentStep_ > 0) --currentStep_;
        }),
        ftxui::Button(" Next > ", [this] {
            if (currentStep_ < 4) ++currentStep_;
        }),
        ftxui::Button(" Generate ", [this] {
            // Phase 6: wire to ModernCMakeGenerator + ProjectRepository.
        }),
    });
}

} // namespace lazycmake::tui
