#include "lazycmake/tui/wizard_screen.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/cmake_gen/modern_cmake_generator.hpp"
#include "lazycmake/core/project_repository.hpp"

namespace lazycmake::tui {

namespace {

const std::vector<std::string> kLanguages = {"C", "C++", "Both"};
const std::vector<std::string> kTargetTypes = {"Executable", "Static Library", "Shared Library"};
const std::vector<std::string> kCppStandards = {"C++17", "C++20", "C++23"};
const std::vector<std::string> kGenerators = {"Ninja", "Unix Makefiles"};
const std::vector<std::string> kDeps = {"fmt", "spdlog", "nlohmann_json", "Catch2", "Boost.Asio"};

} // namespace

WizardScreen::WizardScreen(config::KeymapManager& keymap)
    : keymap_(keymap) {
    name = "wizard";

    // Initialize dependency checkboxes with shared_ptr to avoid memory leaks.
    for (size_t i = 0; i < kDeps.size(); ++i) {
        depChecked_.push_back(std::make_shared<bool>(false));
    }

    std::vector<ftxui::Component> steps;
    steps.push_back(buildStep0());
    steps.push_back(buildStep1());
    steps.push_back(buildStep2());
    steps.push_back(buildStep3());
    steps.push_back(buildStep4());

    auto nav = buildNavigation();

    auto tabContainer = ftxui::Container::Tab(steps, &currentStep_);
    // The tab container must be inside a Container::Vertical so that
    // navigation buttons stay visible below the current step.
    auto whole = ftxui::Container::Vertical({
        tabContainer,
        nav,
    });

    // Wrap with keyboard event handling.
    // We do NOT intercept Tab here — let FTXUI handle focus movement
    // naturally. Use arrow keys + Enter on buttons to navigate steps.
    component = ftxui::CatchEvent(whole, [this](ftxui::Event e) {
        if (e == ftxui::Event::Escape) {
            if (onCancel_) onCancel_();
            return true;
        }
        return false;
    });
}

ftxui::Component WizardScreen::buildStep0() {
    auto nameInput = ftxui::Input(&projectName_, "MyProject");
    auto dirInput = ftxui::Input(&projectDir_, ".");
    auto langToggle = ftxui::Toggle(&kLanguages, &languageIdx_);

    return ftxui::Container::Vertical({
        ftxui::Renderer([this] {
            return ftxui::vbox({
                ftxui::text("Step 1: Project Name & Language") | ftxui::bold,
                ftxui::separator(),
            });
        }),
        ftxui::Container::Vertical({
            ftxui::Container::Horizontal({
                ftxui::Renderer([] { return ftxui::text(" Project Name: "); }),
                nameInput,
            }),
            ftxui::Container::Horizontal({
                ftxui::Renderer([] { return ftxui::text(" Directory:     "); }),
                dirInput,
            }),
            ftxui::Container::Horizontal({
                ftxui::Renderer([] { return ftxui::text(" Language:      "); }),
                langToggle,
            }),
        }),
    });
}

ftxui::Component WizardScreen::buildStep1() {
    auto toggle = ftxui::Toggle(&kTargetTypes, &targetTypeIdx_);
    return ftxui::Container::Vertical({
        ftxui::Renderer([this] {
            return ftxui::vbox({
                ftxui::text("Step 2: Target Type") | ftxui::bold,
                ftxui::separator(),
                ftxui::text("Select the type of target to build:"),
            });
        }),
        toggle,
    });
}

ftxui::Component WizardScreen::buildStep2() {
    auto toggle = ftxui::Toggle(&kCppStandards, &cppStdIdx_);
    return ftxui::Container::Vertical({
        ftxui::Renderer([this] {
            return ftxui::vbox({
                ftxui::text("Step 3: C++ Standard") | ftxui::bold,
                ftxui::separator(),
                ftxui::text("Select the C++ standard to use:"),
            });
        }),
        toggle,
    });
}

ftxui::Component WizardScreen::buildStep3() {
    auto toggle = ftxui::Toggle(&kGenerators, &generatorIdx_);
    return ftxui::Container::Vertical({
        ftxui::Renderer([this] {
            return ftxui::vbox({
                ftxui::text("Step 4: Build System") | ftxui::bold,
                ftxui::separator(),
                ftxui::text("Select the build system generator:"),
            });
        }),
        toggle,
    });
}

ftxui::Component WizardScreen::buildStep4() {
    std::vector<ftxui::Component> checkboxes;
    for (size_t i = 0; i < kDeps.size(); ++i) {
        checkboxes.push_back(ftxui::Checkbox(kDeps[i], depChecked_[i].get()));
    }
    auto list = ftxui::Container::Vertical(std::move(checkboxes));

    return ftxui::Container::Vertical({
        ftxui::Renderer([this] {
            return ftxui::vbox({
                ftxui::text("Step 5: Dependencies") | ftxui::bold,
                ftxui::separator(),
                ftxui::text("Check the dependencies to include (Tab to navigate, Space to toggle):"),
            });
        }),
        list,
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
            auto project = collectProject();
            if (onGenerate_) onGenerate_(std::move(project));
        }),
    });
}

core::Project WizardScreen::collectProject() {
    core::Project proj;
    proj.name = projectName_;
    proj.rootDir = std::filesystem::absolute(projectDir_);

    if (languageIdx_ == 0) proj.language = core::Language::C;
    else if (languageIdx_ == 1) proj.language = core::Language::Cpp;
    else proj.language = core::Language::Both;

    if (languageIdx_ != 0) {
        if (cppStdIdx_ == 0) proj.cppStandard = core::CppStandard::Cpp17;
        else if (cppStdIdx_ == 1) proj.cppStandard = core::CppStandard::Cpp20;
        else proj.cppStandard = core::CppStandard::Cpp23;
    }

    if (generatorIdx_ == 0) proj.buildConfig.generator = core::GeneratorKind::Ninja;
    else proj.buildConfig.generator = core::GeneratorKind::UnixMakefiles;

    core::Target mainTarget;
    mainTarget.name = projectName_;
    if (targetTypeIdx_ == 0) mainTarget.kind = core::TargetKind::Executable;
    else if (targetTypeIdx_ == 1) mainTarget.kind = core::TargetKind::StaticLibrary;
    else mainTarget.kind = core::TargetKind::SharedLibrary;
    mainTarget.sources.push_back({"src/main.cpp", false});
    proj.targets.push_back(std::move(mainTarget));

    // Collect enabled dependencies from step 4.
    for (size_t i = 0; i < kDeps.size() && i < depChecked_.size(); ++i) {
        if (*depChecked_[i]) {
            core::DependencySpec spec;
            spec.name = kDeps[i];
            spec.source = core::DependencySource::FetchContent;
            spec.enabled = true;
            proj.dependencies.push_back(std::move(spec));
        }
    }

    return proj;
}

} // namespace lazycmake::tui
