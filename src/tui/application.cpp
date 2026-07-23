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
    return ftxui::CatchEvent(styled, [self, items, selected](ftxui::Event e) {
        if (e == ftxui::Event::Character('q')) {
            self->screen_.Exit();
            return true;
        }
        if (e == ftxui::Event::Character('h')) {
            if (self->overlayState_ && self->overlayState_->helpOverlay) {
                self->overlayState_->helpOverlay->toggle();
            }
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
        case 2:
            spdlog::info("Startup: Recent Projects not yet implemented");
            break;
        case 3: navigateToTemplates(); break;
        case 4: navigateToSettings(); break;
        case 5: screen_.Exit(); break;
    }
}

void Application::scheduleScreen(ftxui::Component screen) {
    pendingScreen_ = std::move(screen);
    hasPendingScreen_ = true;
    screen_.Post([this] { applyPendingScreen(); });
}

void Application::setScreen(ftxui::Component screen) {
    spdlog::debug("Screen: applying new component tree");

    screenRoot_->DetachAllChildren();

    auto stacked = ftxui::Container::Stacked({
        std::move(screen),
    });

    screenRoot_->Add(std::move(stacked));
    spdlog::debug("Screen: component switch complete");
}

void Application::applyPendingScreen() {
    if (hasPendingScreen_ && pendingScreen_) {
        setScreen(std::move(pendingScreen_));
        hasPendingScreen_ = false;
    }
}

void Application::navigateToStartup() {
    spdlog::info("Screen: navigating to startup");
    // Fix #10: defer destruction via Post() to avoid self-destruction
    // during a callback owned by the object being destroyed.
    screen_.Post([this] {
        screenState_.reset();
        overlayState_.reset();
        scheduleScreen(makeStartupScreen());
    });
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
                self->screen_.Exit();
                return true;
            }
            if (e == ftxui::Event::Escape) {
                spdlog::debug("Wizard: Escape pressed, cancel");
                self->navigateToStartup();
                return true;
            }
            return false;
        });

    screenState_ = std::move(screen);
    scheduleScreen(std::move(withKeys));
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

    scheduleScreen(std::move(component));
}

void Application::navigateToTemplates() {
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
    scheduleScreen(std::move(withKeys));
}

void Application::navigateToWorkspace(core::Project project) {
    spdlog::info("Screen: navigating to workspace for '{}'", project.name);
    auto self = this;

    auto buildOverlay = std::make_shared<BuildOverlay>(
        eventBus_, keymap_, theme_, buildManager_);
    buildOverlay->setBuildDir(project.rootDir.string());
    auto runOverlay = std::make_shared<RunOverlay>(
        eventBus_, keymap_, theme_, runManager_);
    auto depDialog = std::make_shared<DependencyDialog>(
        keymap_, theme_, dependencyRegistry_, project);
    auto conflictDialog = std::make_shared<ConflictDialog>(
        keymap_, theme_);
    auto helpOverlay = std::make_shared<HelpOverlay>(keymap_);

    auto workspace = std::make_unique<MainWorkspace>(keymap_, theme_);
    workspace->setProject(project);
    workspace->setEventBus(eventBus_);

    workspace->setOnBuild([self, buildOverlay] {
        buildOverlay->toggle();
    });
    workspace->setOnRun([self, runOverlay, projectName = project.name] {
        runOverlay->setExecutable("./build/" + projectName);
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

    auto buildComp = buildOverlay->build();
    auto runComp = runOverlay->build();
    auto depComp = depDialog->build();
    auto conflictComp = conflictDialog->build();
    auto helpComp = helpOverlay->build();

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
    scheduleScreen(std::move(layered));
}

void Application::onGenerateProject(core::Project project) {
    project_ = project;

    std::error_code ec;
    std::filesystem::create_directories(project.rootDir / "src", ec);
    std::filesystem::create_directories(project.rootDir / "include", ec);
    if (ec) {
        spdlog::error("Failed to create project directories: {}", ec.message());
    }

    cmake_gen::ModernCMakeGenerator generator;
    auto genFiles = generator.generate(project);
    spdlog::info("Generated {} files for project '{}'",
                 genFiles.size(), project.name);

    core::GeneratedFileLock fileLock(
        project.rootDir / ".lazycmake" / "generated.lock.json",
        project.rootDir);
    fileLock.load();

    for (const auto& [relPath, content] : genFiles.files()) {
        auto fullPath = project.rootDir / relPath;
        std::filesystem::create_directories(fullPath.parent_path(), ec);

        if (fileLock.isUserOwned(relPath)) {
            spdlog::info("  Skipping {} (user-owned)", relPath.generic_string());
            continue;
        }

        auto conflict = fileLock.checkConflict(relPath, content);
        if (conflict.hasConflict) {
            spdlog::warn("  Conflict detected for {}, skipping (use conflict dialog in workspace to resolve)", relPath.generic_string());
            continue;
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

    fileLock.save();

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
    spdlog::set_level(spdlog::level::warn);
    loadConfig();

    // Fix #7: wire the EventBus to wake the FTXUI screen after thread-safe
    // publishes so the UI repaints with streamed output.
    eventBus_.setOnEventQueued([this] {
        screen_.Post([]{});  // wake the event loop to drain the queue
    });

    auto drainer = ftxui::Renderer([this] {
        eventBus_.drainQueue();
        return ftxui::emptyElement();
    });

    screenRoot_ = ftxui::Container::Vertical({
        drainer,
    });

    navigateToStartup();
    applyPendingScreen();

    screen_.Loop(screenRoot_);
    return 0;
}

} // namespace lazycmake::tui
