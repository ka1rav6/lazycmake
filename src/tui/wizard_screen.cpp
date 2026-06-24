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

const std::vector<std::string> kDummyDeps = {"fmt", "spdlog", "nlohmann_json", "Catch2", "Boost.Asio"};

} // namespace

WizardScreen::WizardScreen(config::KeymapManager& keymap)
    : keymap_(keymap) {
    name = "wizard";

    std::vector<ftxui::Component> steps;
    steps.push_back(buildStep0());
    steps.push_back(buildStep1());
    steps.push_back(buildStep2());
    steps.push_back(buildStep3());
    steps.push_back(buildStep4());

    auto nav = buildNavigation();

    container_ = ftxui::Container::Vertical({
        ftxui::Container::Tab(steps, &currentStep_),
        nav,
    });

    component = container_;
}

ftxui::Component WizardScreen::buildStep0() {
    auto nameInput = ftxui::Input(&projectName_, "MyProject");
    auto dirInput = ftxui::Input(&projectDir_, ".");
    auto langToggle = ftxui::Toggle(&kLanguages, &languageIdx_);

    auto nameLabel = ftxui::Renderer(std::function<ftxui::Element()>([this] {
        return ftxui::text("Project Name:");
    }));
    auto dirLabel = ftxui::Renderer(std::function<ftxui::Element()>([this] {
        return ftxui::text("Directory:");
    }));
    auto langLabel = ftxui::Renderer(std::function<ftxui::Element()>([this] {
        return ftxui::text("Language:");
    }));

    return ftxui::Container::Vertical({
        ftxui::Container::Horizontal({nameLabel, nameInput}),
        ftxui::Container::Horizontal({dirLabel, dirInput}),
        ftxui::Container::Horizontal({langLabel, langToggle}),
    });
}

ftxui::Component WizardScreen::buildStep1() {
    auto label = ftxui::Renderer(std::function<ftxui::Element()>([this] {
        return ftxui::text("Target Type:");
    }));
    auto toggle = ftxui::Toggle(&kTargetTypes, &targetTypeIdx_);
    return ftxui::Container::Vertical({label, toggle});
}

ftxui::Component WizardScreen::buildStep2() {
    auto label = ftxui::Renderer(std::function<ftxui::Element()>([this] {
        return ftxui::text("C++ Standard:");
    }));
    auto toggle = ftxui::Toggle(&kCppStandards, &cppStdIdx_);
    return ftxui::Container::Vertical({label, toggle});
}

ftxui::Component WizardScreen::buildStep3() {
    auto label = ftxui::Renderer(std::function<ftxui::Element()>([this] {
        return ftxui::text("Build System:");
    }));
    auto toggle = ftxui::Toggle(&kGenerators, &generatorIdx_);
    return ftxui::Container::Vertical({label, toggle});
}

ftxui::Component WizardScreen::buildStep4() {
    auto label = ftxui::Renderer(std::function<ftxui::Element()>([this] {
        return ftxui::text("Dependencies (check to add):");
    }));
    std::vector<ftxui::Component> deps;
    for (size_t i = 0; i < kDummyDeps.size(); ++i) {
        auto* checked = new bool(false);
        deps.push_back(ftxui::Checkbox(kDummyDeps[i], checked));
    }
    auto list = ftxui::Container::Vertical(std::move(deps));
    return ftxui::Container::Vertical({label, list});
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
    else proj.language = core::Language::Cpp;

    if (cppStdIdx_ == 0) proj.cppStandard = core::CppStandard::Cpp17;
    else if (cppStdIdx_ == 1) proj.cppStandard = core::CppStandard::Cpp20;
    else proj.cppStandard = core::CppStandard::Cpp23;

    if (generatorIdx_ == 0) proj.buildConfig.generator = core::GeneratorKind::Ninja;
    else proj.buildConfig.generator = core::GeneratorKind::UnixMakefiles;

    core::Target mainTarget;
    mainTarget.name = projectName_;
    if (targetTypeIdx_ == 0) mainTarget.kind = core::TargetKind::Executable;
    else if (targetTypeIdx_ == 1) mainTarget.kind = core::TargetKind::StaticLibrary;
    else mainTarget.kind = core::TargetKind::SharedLibrary;
    mainTarget.sources.push_back({"src/main.cpp", false});
    proj.targets.push_back(std::move(mainTarget));

    return proj;
}

} // namespace lazycmake::tui
