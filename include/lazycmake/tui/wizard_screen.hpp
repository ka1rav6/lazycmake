#pragma once

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// New Project Wizard with 5 steps: Name/Language, Type, Standard, Build, Deps.
class WizardScreen : public Screen {
public:
    explicit WizardScreen(config::KeymapManager& keymap);
    ~WizardScreen() override = default;

    int currentStep() const { return currentStep_; }

private:
    ftxui::Component buildStep(int step);
    ftxui::Component buildNavigation();
    bool onEvent(ftxui::Event event);

    config::KeymapManager& keymap_;
    int currentStep_ = 0;
    ftxui::Component container_;
};

} // namespace lazycmake::tui
