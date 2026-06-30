#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

#include "lazycmake/config/keymap_manager.hpp"
#include "lazycmake/core/project.hpp"
#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

// New Project Wizard with 5 steps: Name/Language, Type, Standard, Build, Deps.
class WizardScreen : public Screen {
public:
    explicit WizardScreen(config::KeymapManager& keymap);
    ~WizardScreen() override = default;

    using GenerateCallback = std::function<void(core::Project)>;
    void setOnGenerate(GenerateCallback cb) { onGenerate_ = std::move(cb); }
    void setOnCancel(std::function<void()> cb) { onCancel_ = std::move(cb); }

    int currentStep() const { return currentStep_; }

private:
    ftxui::Component buildStep0();  // Name & Language
    ftxui::Component buildStep1();  // Project Type
    ftxui::Component buildStep2();  // C++ Standard
    ftxui::Component buildStep3();  // Build system
    ftxui::Component buildStep4();  // Dependencies (checkboxes)
    ftxui::Component buildNavigation();
    core::Project collectProject();

    config::KeymapManager& keymap_;
    int currentStep_ = 0;
    ftxui::Component container_;

    // Step 0: Name & Language
    std::string projectName_ = "MyProject";
    std::string projectDir_ = ".";
    int languageIdx_ = 1; // 0=C, 1=C++, 2=Both

    // Step 1: Target type
    int targetTypeIdx_ = 0; // 0=Executable, 1=Static, 2=Shared

    // Step 2: C++ standard
    int cppStdIdx_ = 1; // 0=C++17, 1=C++20, 2=C++23

    // Step 3: Build system
    int generatorIdx_ = 0; // 0=Ninja, 1=Unix Makefiles

    // Step 4: Dependencies (shared_ptr to avoid memory leaks with FTXUI checkboxes)
    std::vector<std::shared_ptr<bool>> depChecked_;

    // Callbacks
    GenerateCallback onGenerate_;
    std::function<void()> onCancel_;
};

} // namespace lazycmake::tui
