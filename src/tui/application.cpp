#include "lazycmake/tui/application.hpp"

#include <fstream>

#include <spdlog/spdlog.h>

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include "lazycmake/cmake_gen/modern_cmake_generator.hpp"
#include "lazycmake/config/settings_manager.hpp"
#include "lazycmake/core/generated_file_lock.hpp"
#include "lazycmake/core/project_repository.hpp"
#include "lazycmake/tui/build_overlay.hpp"
#include "lazycmake/tui/color_helper.hpp"
#include "lazycmake/tui/conflict_dialog.hpp"
#include "lazycmake/tui/dependency_dialog.hpp"
#include "lazycmake/tui/help_overlay.hpp"
#include "lazycmake/tui/main_workspace.hpp"
#include "lazycmake/tui/run_overlay.hpp"
#include "lazycmake/tui/settings_screen.hpp"
#include "lazycmake/tui/wizard_screen.hpp"

namespace lazycmake::tui {

Application::Application()
    : screen_(ftxui::ScreenInteractive::Fullscreen()),
      buildManager_(std::make_unique<build::FakeBuildBackend>(), eventBus_),
      runManager_(eventBus_) {}

Application::~Application() = default;

void Application::loadConfig() {
    settings_.loadAll();
    keymap_.loadAll();
    theme_.loadAll();
    spdlog::info("Configuration loaded");
}

ftxui::Component Application::makeStartupScreen() {
    auto self = this;

    // We store state as shared_ptr in the component captures so they
    // stay valid even after screen transitions and returns.
    auto items = std::make_shared<std::vector<std::string>>(
        std::vector<std::string>{
            "New Project", "Open Project", "Recent Projects",
            "Templates",   "Settings",     "Quit"});
    auto selected = std::make_shared<int>(0);

    auto menu = ftxui::Menu(items.get(), selected.get());

    auto styled = ftxui::Renderer([self, items, selected, menu] {
        const auto& colors = self->theme_.activeColors();

        auto title = ftxui::vbox({
            ftxui::text("LazyCMake") | ftxui::bold | ftxui::center,
            ftxui::text("lazygit, but for CMake") | ftxui::center
                | ftxui::dim,
        });

        auto footer =
            ftxui::text("j/k  navigate  enter  select  q  quit  h  help")
            | ftxui::center | ftxui::dim;

        return ftxui::vbox({
                   title,
                   ftxui::separator(),
                   menu->Render(),
                   ftxui::separator(),
                   footer,
               }) |
               ftxui::bgcolor(colorFromString(colors.background)) |
               ftxui::color(colorFromString(colors.foreground));
    });
    styled->Add(menu);

    return ftxui::CatchEvent(styled, [self, items, selected](ftxui::Event e) {
        if (e == ftxui::Event::Character('q')) {
            self->screen_.ExitLoopClosure()();
            return true;
        }
        if (e == ftxui::Event::Character('j') ||
            e == ftxui::Event::ArrowDown) {
            *selected = std::min(*selected + 1,
                                 static_cast<int>(items->size()) - 1);
            return true;
        }
        if (e == ftxui::Event::Character('k') ||
            e == ftxui::Event::ArrowUp) {
            *selected = std::max(*selected - 1, 0);
            return true;
        }
        if (e == ftxui::Event::Return) {
            self->onStartupAction(*selected);
            return true;
        }
        return false;
    });
}

void Application::onStartupAction(int idx) {
    switch (idx) {
        case 0: navigateToWizard(); break;
        case 1: navigateToOpen(); break;
        case 3: navigateToTemplates(); break;
        case 4: navigateToSettings(); break;
        case 5: screen_.ExitLoopClosure()(); break;
    }
}

void Application::setScreen(ftxui::Component screen) {
    spdlog::debug("Screen: setScreen called, switching component");

    screenRoot_->DetachAllChildren();

    // Drainer sits at the bottom of the stack, invisible and event-transparent.
    auto drainer = ftxui::Renderer([this] {
        eventBus_.drainQueue();
        return ftxui::emptyElement();
    });

    // Stacked renders children on top of each other.
    // Events go to children in reverse order (topmost first).
    // drainer is at index 0 (bottom) — never handles events.
    // screen is at index 1 (top) — receives events normally through the FTXUI tree.
    auto stacked = ftxui::Container::Stacked({
        std::move(drainer),
        std::move(screen),
    });

    screenRoot_->Add(std::move(stacked));
    spdlog::debug("Screen: component switch complete");
}

void Application::navigateToStartup() {
    spdlog::info("Screen: navigating to startup");
    screenState_.reset();
    overlayState_.reset();
    setScreen(makeStartupScreen());
}

void Application::navigateToWizard() {
    spdlog::info("Screen: navigating to wizard");
    auto self = this;
    auto screen = std::make_unique<WizardScreen>(keymap_);

    screen->setOnGenerate([self](core::Project project) {
        spdlog::info("Wizard: onGenerate fired");
        self->onGenerateProject(std::move(project));
    });
    screen->setOnCancel([self] {
        spdlog::info("Wizard: onCancel fired");
        self->navigateToStartup();
    });

    auto inner = screen->component;
    auto withKeys =
        ftxui::CatchEvent(std::move(inner), [self](ftxui::Event e) {
            if (e == ftxui::Event::Character('q')) {
                spdlog::info("Wizard: 'q' pressed, exiting program");
                self->screen_.ExitLoopClosure()();
                return true;
            }
            return false;
        });

    screenState_ = std::move(screen);
    setScreen(std::move(withKeys));
}

void Application::navigateToOpen() {
    spdlog::info("OpenProject: scanning for projects");
    auto self = this;

    auto entryLabels = std::make_shared<std::vector<std::string>>();
    auto entryPaths = std::make_shared<std::vector<std::filesystem::path>>();
    auto selected = std::make_shared<int>(0);

    auto searchDir = [&](const std::filesystem::path& dir) {
        std::error_code ec;
        auto manifest = dir / ".lazycmake" / "project.json";
        if (std::filesystem::exists(manifest, ec)) {
            entryLabels->push_back(dir.filename().string());
            entryPaths->push_back(dir);
            spdlog::debug("OpenProject: found '{}' at {}",
                          dir.filename().string(), dir.string());
        }
    };

    searchDir(std::filesystem::current_path());
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path(), ec)) {
        if (entry.is_directory()) {
            searchDir(entry.path());
        }
    }

    if (entryLabels->empty()) {
        entryLabels->push_back("(no projects found - press q or esc to go back)");
        spdlog::info("OpenProject: no projects found");
    } else {
        spdlog::info("OpenProject: found {} project(s)", entryPaths->size());
    }

    // Menu handles focus natively so j/k/arrows always work.
    auto menu = ftxui::Menu(entryLabels.get(), selected.get());

    auto container = ftxui::Container::Vertical({
        ftxui::Renderer([self, entryLabels, menu] {
            const auto& colors = self->theme_.activeColors();
            return ftxui::vbox({
                       ftxui::text(" Open Project ") | ftxui::bold | ftxui::center,
                       ftxui::separator(),
                       menu->Render(),
                       ftxui::separator(),
                       ftxui::text(" j/k: navigate   enter: open   q: back ")
                           | ftxui::center | ftxui::dim,
                   }) |
                   ftxui::bgcolor(colorFromString(colors.background)) |
                   ftxui::color(colorFromString(colors.foreground));
        }),
    });
    container->Add(menu);

    auto component = ftxui::CatchEvent(container, [self, entryPaths, selected](ftxui::Event e) {
        if (e == ftxui::Event::Character('q') || e == ftxui::Event::Escape) {
            spdlog::debug("OpenProject: going back to startup");
            self->navigateToStartup();
            return true;
        }
        if (e == ftxui::Event::Return) {
            if (entryPaths->empty()) {
                spdlog::debug("OpenProject: enter with no projects, ignoring");
                return true;
            }
            auto path = entryPaths->at(*selected);
            spdlog::info("OpenProject: loading project at {}", path.string());
            core::ProjectRepository repo;
            auto result = repo.load(path);
            if (result) {
                self->project_ = std::move(result.value());
                spdlog::info("OpenProject: loaded '{}'", self->project_.name);
                self->navigateToWorkspace(self->project_);
            } else {
                spdlog::error("OpenProject: load failed: {}", result.error().message);
            }
            return true;
        }
        return false;
    });

    setScreen(std::move(component));
}

void Application::navigateToTemplates() {
    // Stub: template selection.
    navigateToStartup();
}

void Application::navigateToSettings() {
    spdlog::info("Screen: navigating to settings");
    auto self = this;
    auto screen = std::make_unique<SettingsScreen>(keymap_, settings_);

    auto inner = screen->component;
    auto withKeys =
        ftxui::CatchEvent(std::move(inner), [self](ftxui::Event e) {
            if (e == ftxui::Event::Character('q') ||
                e == ftxui::Event::Escape) {
                spdlog::debug("Settings: going back to startup");
                self->navigateToStartup();
                return true;
            }
            return false;
        });

    screenState_ = std::move(screen);
    setScreen(std::move(withKeys));
}

void Application::navigateToWorkspace(core::Project project) {
    spdlog::info("Screen: navigating to workspace for '{}'", project.name);
    auto self = this;

    // Create overlays.
    auto buildOverlay = std::make_shared<BuildOverlay>(
        eventBus_, keymap_, theme_, buildManager_);
    auto runOverlay = std::make_shared<RunOverlay>(
        eventBus_, keymap_, theme_, runManager_);
    auto depDialog = std::make_shared<DependencyDialog>(
        keymap_, theme_, dependencyRegistry_, project_);
    auto conflictDialog = std::make_shared<ConflictDialog>(
        keymap_, theme_);
    auto helpOverlay = std::make_shared<HelpOverlay>(keymap_);

    // Create main workspace.
    auto workspace = std::make_unique<MainWorkspace>(keymap_, theme_);
    workspace->setProject(project);
    workspace->setEventBus(eventBus_);

    // Wire workspace callbacks.
    workspace->setOnBuild([self, buildOverlay] {
        buildOverlay->toggle();
    });
    workspace->setOnRun([self, runOverlay] {
        runOverlay->setExecutable("./build/" + self->project_.name);
        runOverlay->toggle();
    });
    workspace->setOnDeps([self, depDialog] {
        depDialog->toggle();
    });
    workspace->setOnHelp([self, helpOverlay] {
        helpOverlay->toggle();
    });
    workspace->setOnExit([self] {
        self->navigateToStartup();
    });

    // Build overlays.
    auto buildComp = buildOverlay->build();
    auto runComp = runOverlay->build();
    auto depComp = depDialog->build();
    auto conflictComp = conflictDialog->build();
    auto helpComp = helpOverlay->build();

    // Stack workspace + overlays using Container::Tab or layering.
    // FTXUI doesn't have a native overlay; we layer by stacking
    // components. The approach: use a Container::Stacked (which layers
    // children on top of each other). Each overlay is rendered on top
    // of the workspace when visible.
    auto layered = ftxui::Container::Stacked({
        workspace->component,
        buildComp,
        runComp,
        depComp,
        conflictComp,
        helpComp,
    });

    overlayState_ = std::make_unique<OverlayState>();
    overlayState_->buildOverlay = buildOverlay;
    overlayState_->runOverlay = runOverlay;
    overlayState_->depDialog = depDialog;
    overlayState_->helpOverlay = helpOverlay;
    overlayState_->conflictDialog = conflictDialog;

    screenState_ = std::move(workspace);
    setScreen(std::move(layered));
}

void Application::onGenerateProject(core::Project project) {
    project_ = project;

    // Create project directory structure.
    std::error_code ec;
    std::filesystem::create_directories(project.rootDir / "src", ec);
    std::filesystem::create_directories(project.rootDir / "include", ec);
    if (ec) {
        spdlog::error("Failed to create project directories: {}", ec.message());
    }

    // Generate CMakeLists.txt and related files.
    cmake_gen::ModernCMakeGenerator generator;
    auto genFiles = generator.generate(project);
    spdlog::info("Generated {} files for project '{}'",
                 genFiles.size(), project.name);

    // Load the file lock to detect hand-edited files.
    core::GeneratedFileLock fileLock(
        project.rootDir / ".lazycmake" / "generated.lock.json",
        project.rootDir);
    fileLock.load();

    // Write generated files to disk.
    for (const auto& [relPath, content] : genFiles.files()) {
        auto fullPath = project.rootDir / relPath;
        std::filesystem::create_directories(fullPath.parent_path(), ec);

        // Skip files the user has marked as owned.
        if (fileLock.isUserOwned(relPath)) {
            spdlog::info("  Skipping {} (user-owned)", relPath.generic_string());
            continue;
        }

        // Check for conflict (on-disk hash differs from stored hash).
        auto conflict = fileLock.checkConflict(relPath, content);
        if (conflict.hasConflict) {
            spdlog::warn("  Conflict detected for {}, overwriting", relPath.generic_string());
            // TODO: show conflict dialog for interactive resolution
        }

        std::ofstream outFile(fullPath);
        if (outFile.is_open()) {
            outFile << content;
            fileLock.recordHash(relPath, content);
            spdlog::info("  Wrote {}", relPath.generic_string());
        } else {
            spdlog::error("  Failed to write {}", relPath.generic_string());
        }
    }

    // Save the lock file so future generations can detect changes.
    fileLock.save();

    // Create src/main.cpp stub if it doesn't exist.
    auto mainCppPath = project.rootDir / "src" / "main.cpp";
    if (!std::filesystem::exists(mainCppPath)) {
        std::ofstream mainFile(mainCppPath);
        if (mainFile.is_open()) {
            std::string lang = (project.language == core::Language::C) ? "C" : "C++";
            mainFile << "#include <iostream>\n\n";
            mainFile << "int main() {\n";
            mainFile << "    std::cout << \"Hello from " << project.name
                     << "!\" << std::endl;\n";
            mainFile << "    return 0;\n";
            mainFile << "}\n";
            spdlog::info("  Created src/main.cpp");
        }
    }

    // Save the project manifest.
    core::ProjectRepository repo;
    auto result = repo.save(project);
    if (result) {
        spdlog::info("Project '{}' saved at {}",
                     project.name, project.rootDir.string());
    } else {
        spdlog::error("Failed to save project: {}",
                      result.error().message);
    }

    navigateToWorkspace(std::move(project));
}

void Application::drainEventQueue() {
    eventBus_.drainQueue();
}

int Application::run(int /*argc*/, char** /*argv*/) {
    spdlog::set_level(spdlog::level::info);
    loadConfig();

    screenRoot_ = ftxui::Container::Vertical({});
    navigateToStartup();
    screen_.Loop(screenRoot_);
    return 0;
}

} // namespace lazycmake::tui
