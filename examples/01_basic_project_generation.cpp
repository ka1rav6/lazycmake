// ==========================================================================
// Example 1: Basic Project Creation & CMake Generation
//
// This example demonstrates the core workflow:
//   1. Create a Project with targets and dependencies
//   2. Validate the target graph
//   3. Generate CMakeLists.txt files (Phase 2)
//   4. Print the generated files to stdout
//   5. Save the project manifest to disk
//
// To build and run:
//   cd lazycmake
//   g++ -std=c++20 -I include \
//       examples/01_basic_project_generation.cpp \
//       src/core/types.cpp src/core/project.cpp \
//       src/core/target_validator.cpp src/core/project_repository.cpp \
//       src/cmake_gen/*.cpp src/config/config_types.cpp \
//       -o /tmp/lazycmake_example1 \
//       -l nlohmann_json -l fmt -l spdlog
//   /tmp/lazycmake_example1
//
// Or just run it through the existing test binary for simpler execution.
// ==========================================================================

#include <iostream>
#include <filesystem>

// Core: the Project data model, the single source of truth.
// Everything else (CMake generation, validation, persistence) is a
// pure function of this struct.
#include "lazycmake/core/project.hpp"

// TargetValidator: checks for duplicate names, cyclic links, etc.
// Always validate before generating — it catches errors that would
// only surface as confusing CMake configure errors.
#include "lazycmake/core/target_validator.hpp"

// ProjectRepository: saves/loads Project to/from .lazycmake/project.json.
// This is the ONLY class that touches the manifest file on disk.
#include "lazycmake/core/project_repository.hpp"

// ModernCMakeGenerator: the default CMake generator.
// Takes a Project, produces a GeneratedFileSet (in-memory file map).
#include "lazycmake/cmake_gen/modern_cmake_generator.hpp"

// The GeneratedFileSet type — an in-memory map of relative path → content.
#include "lazycmake/cmake_gen/generated_file_set.hpp"

// Convenience: bring all of lazycmake into scope for the example.
using namespace lazycmake::core;
using namespace lazycmake::cmake_gen;

int main() {
    // ======================================================================
    // Step 1: Build a Project in memory
    //
    // The Project struct is a plain aggregate — no constructors to call,
    // no factories. Just set the fields you need. Defaults are sensible
    // (C++ language, Ninja generator, Debug build type).
    // ======================================================================

    std::cout << "=== LazyCMake Example 1: Basic Project Generation ===\n\n";

    Project myGame;
    myGame.name = "MyGame";
    myGame.language = Language::Cpp;          // Also: C, Both
    myGame.cppStandard = CppStandard::Cpp20;  // Also: Cpp17, Cpp23
    myGame.rootDir = "/tmp/my_game_project";  // Where the project lives on disk

    // ---- Add targets ----
    // Targets are the build targets (executables, libraries).
    // Each target has a name, kind, source files, include dirs, and
    // links to other targets or dependencies.

    // Add an engine library (static library).
    Target engine;
    engine.name = "engine";
    engine.kind = TargetKind::StaticLibrary;
    engine.sources.push_back(SourceFileRef{
        .path = "src/Scene.cpp",
        .autoDetected = false,        // false = user-added (vs FileWatcher)
    });
    engine.sources.push_back(SourceFileRef{
        .path = "src/Renderer.cpp",
        .autoDetected = true,         // true = auto-detected by FileWatcher
    });
    engine.includeDirs.push_back("include");  // Public headers
    engine.compileDefinitions["ENGINE_BUILD"] = "1";  // -DENGINE_BUILD=1
    myGame.targets.push_back(engine);

    // Add the main executable.
    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "src/main.cpp"});
    app.linkTargets.push_back("engine");    // Links to our "engine" library
    app.cppStandard = CppStandard::Cpp20;   // Override project default
    myGame.targets.push_back(app);

    // ---- Add dependencies ----
    // Dependencies are external libraries fetched at configure time.
    // Each has a source (FetchContent, CPM, vcpkg, or system find_package).
    DependencySpec fmtDep;
    fmtDep.name = "fmt";
    fmtDep.source = DependencySource::FetchContent;
    fmtDep.gitRepo = "https://github.com/fmtlib/fmt.git";
    fmtDep.gitTag = "11.0.2";
    fmtDep.linkTargets = {"fmt::fmt"};      // The CMake target name
    fmtDep.enabled = true;                   // Active in the build
    myGame.dependencies.push_back(fmtDep);

    // Link fmt to the app target.
    app.linkTargets.push_back("fmt::fmt");
    // Note: since we've already pushed app to myGame.targets above, we need
    // to update the target in-place. Alternatively, we could set up all
    // the data first, then push. Let's fix this:
    myGame.targets.back().linkTargets.push_back("fmt::fmt");

    // ---- Set build configuration ----
    myGame.buildConfig.generator = GeneratorKind::Ninja;
    myGame.buildConfig.defaultBuildType = BuildType::Debug;
    myGame.buildConfig.buildDir = "build";

    // ======================================================================
    // Step 2: Validate the target graph
    //
    // Always validate before generating. TargetValidator catches:
    //   - Duplicate target names
    //   - Targets linking to themselves
    //   - Unknown link targets (typos)
    //   - Cyclic link dependency graphs
    //   - Targets with no source files
    // ======================================================================

    std::cout << "Validating project...\n";
    auto issues = TargetValidator::validate(myGame);

    if (!issues.empty()) {
        std::cout << "Validation found " << issues.size() << " issue(s):\n";
        for (const auto& issue : issues) {
            std::cout << "  [" << (issue.severity == ValidationSeverity::Error ? "ERROR" : "WARN")
                      << "] " << issue.message << "\n";
        }
        std::cout << "Fix the errors above before generating.\n";
        return 1;
    }
    std::cout << "  ✓ No validation issues found\n\n";

    // ======================================================================
    // Step 3: Generate CMakeLists.txt files
    //
    // The ModernCMakeGenerator produces a complete set of CMake files in
    // memory. No files are written to disk yet — this is a pure function
    // from Project → map<path, string>.
    //
    // Generated file structure:
    //   CMakeLists.txt               — root: cmake_minimum_required, project(), add_subdirectory()
    //   cmake/dependencies.cmake     — FetchContent_Declare / find_package blocks
    //   engine/CMakeLists.txt        — add_library(engine STATIC ...) + target_* calls
    //   app/CMakeLists.txt           — add_executable(app ...) + target_* calls
    // ======================================================================

    std::cout << "Generating CMake files...\n";
    ModernCMakeGenerator generator;
    GeneratedFileSet files = generator.generate(myGame);

    std::cout << "  Generated " << files.size() << " file(s):\n\n";

    // ======================================================================
    // Step 4: Print the generated files
    //
    // The GeneratedFileSet is a map of relative path → content.
    // Let's print each file with a header.
    // ======================================================================

    for (const auto& [path, content] : files.files()) {
        std::cout << "──────────────────────────────────────────────\n";
        std::cout << "📄 " << path.generic_string() << "\n";
        std::cout << "──────────────────────────────────────────────\n";
        std::cout << content << "\n\n";
    }

    // ======================================================================
    // Step 5: Save the project manifest to disk
    //
    // ProjectRepository writes the Project model as JSON to
    // <rootDir>/.lazycmake/project.json. This is the on-disk
    // representation that LazyCMake reads on next launch.
    // ======================================================================

    std::cout << "Saving project manifest...\n";
    ProjectRepository repo;
    auto saveResult = repo.save(myGame);

    if (saveResult.has_value()) {
        std::cout << "  ✓ Project saved to "
                  << (myGame.rootDir / ".lazycmake" / "project.json").generic_string()
                  << "\n";
    } else {
        std::cout << "  ✗ Failed to save: " << saveResult.error().message << "\n";
        return 1;
    }

    // ======================================================================
    // Step 6 (bonus): Load it back and verify the round-trip
    // ======================================================================

    std::cout << "\nLoading project back from disk...\n";
    auto loadResult = repo.load(myGame.rootDir);
    if (loadResult.has_value()) {
        const Project& loaded = loadResult.value();
        std::cout << "  ✓ Loaded project: " << loaded.name << "\n";
        std::cout << "  ✓ Targets: " << loaded.targets.size() << "\n";
        std::cout << "  ✓ Dependencies: " << loaded.dependencies.size() << "\n";
        std::cout << "  ✓ rootDir: " << loaded.rootDir.generic_string() << "\n";
    } else {
        std::cout << "  ✗ Failed to load: " << loadResult.error().message << "\n";
    }

    std::cout << "\n=== Done! ===\n";
    return 0;
}
