#include "lazycmake/cmake_gen/modern_cmake_generator.hpp"

#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace lazycmake::cmake_gen {

// ==========================================================================
// ModernCMakeGenerator implementation
//
// This file is the heart of Phase 2. It translates the Core-layer Project
// model (a pure data struct) into a set of CMakeFile ASTs, which are then
// printed to text by CMakePrinter.
//
// The work is split into three AST builders, matching the three kinds of
// files we generate:
//   1. buildRootFile()        → CMakeLists.txt (root)
//   2. buildDependenciesFile() → cmake/dependencies.cmake
//   3. buildTargetFile()      → <target>/CMakeLists.txt (per target)
//
// Each builder returns a CMakeFile (an ordered list of statement elements),
// which the printer renders. This split means:
//   - Each builder is independently testable.
//   - A plugin can hook between building and printing to modify the AST.
//   - The printer's formatting is orthogonal to the logical structure.
// ==========================================================================

ModernCMakeGenerator::ModernCMakeGenerator(CMakePrinter::Options printerOpts)
    : printerOpts_(std::move(printerOpts)) {}

GeneratedFileSet ModernCMakeGenerator::generate(const core::Project& project) const {
    // Create the output set.
    GeneratedFileSet output;

    // Build and print each CMakeFile, then add the result to the output.
    //
    // The CMakePrinter is created per-file (or we could reuse one instance).
    // Each file gets its own printer because different files might want
    // different options (e.g., the root file has the header, target files
    // don't need the same header), but for simplicity we share the options.
    CMakePrinter printer(printerOpts_);

    // ---- 1. Root CMakeLists.txt ----
    CMakeFile rootFile = buildRootFile(project);
    output.addFile("CMakeLists.txt", printer.print(rootFile));

    // ---- 2. cmake/dependencies.cmake (only if there are enabled deps) ----
    bool hasEnabledDeps = std::any_of(
        project.dependencies.begin(), project.dependencies.end(),
        [](const core::DependencySpec& dep) { return dep.enabled; });
    if (hasEnabledDeps) {
        CMakeFile depsFile = buildDependenciesFile(project);
        output.addFile("cmake/dependencies.cmake", printer.print(depsFile));
    }

    // ---- 3. Per-target CMakeLists.txt files ----
    for (const auto& target : project.targets) {
        CMakeFile targetFile = buildTargetFile(project, target);
        // Skip targets that produce no output (e.g., no sources).
        // Even interface libraries get a file — they declare INTERFACE
        // include directories and link libraries. We only skip if the
        // builder returned an empty file (no statements to render).
        if (targetFile.empty()) continue;

        // The target's CMakeLists.txt goes in <target_name>/CMakeLists.txt
        // relative to the project root. This mirrors how most hand-written
        // modern CMake projects organize things: each subsystem gets its
        // own subdirectory, keeping the root file clean and focused on
        // project structure rather than target details.
        std::filesystem::path targetFilePath =
            std::filesystem::path(target.name) / "CMakeLists.txt";
        output.addFile(targetFilePath, printer.print(targetFile));
    }

    return output;
}

// ==========================================================================
// buildRootFile — the root CMakeLists.txt
//
// Layout:
//   cmake_minimum_required(VERSION 3.21)
//   project(<name> LANGUAGES <C|CXX|C CXX>)
//
//   # Dependencies
//   include(cmake/dependencies.cmake)     [if any enabled deps exist]
//
//   # Targets
//   add_subdirectory(<target>)            [for each target with a file]
//
// We hardcode cmake_minimum_required to 3.21 since LazyCMake itself
// requires 3.21 (the version we use for FetchContent and the features
// we generate). A future settings option could make this configurable.
// ==========================================================================

CMakeFile ModernCMakeGenerator::buildRootFile(const core::Project& project) const {
    CMakeFile file;

    // ---- cmake_minimum_required ----
    // This must be the first command in any CMakeLists.txt.
    file.addStatement(CMakeStatement(
        "cmake_minimum_required",
        {"VERSION", "3.21"}
    ));

    // ---- project() ----
    // The project() call names the project and declares which languages
    // are used. We always pass LANGUAGES explicitly (never rely on the
    // default, which is C and CXX) to match what the user configured.
    {
        std::string langArg = buildLanguageArg(project);
        file.addStatement(CMakeStatement(
            "project",
            {project.name, "LANGUAGES", langArg}
        ));
    }

    // ---- Blank line before dependencies section ----
    file.addBlankLine();

    // ---- Optional dependencies include ----
    bool hasEnabledDeps = std::any_of(
        project.dependencies.begin(), project.dependencies.end(),
        [](const core::DependencySpec& dep) { return dep.enabled; });
    if (hasEnabledDeps) {
        file.addComment("Dependencies (FetchContent / find_package)");
        file.addStatement(CMakeStatement(
            "include",
            {"cmake/dependencies.cmake"}
        ));
        file.addBlankLine();
    }

    // ---- add_subdirectory for each target ----
    // InterfaceLibrary targets *do* get a CMakeLists.txt (for their
    // INTERFACE properties), so they get an add_subdirectory too.
    // We skip targets that have no sources and are not interface
    // libraries, since buildTargetFile() would return empty for them.
    file.addComment("Targets");
    for (const auto& target : project.targets) {
        // Skip targets that would produce an empty target file:
        // non-interface targets with no source files.
        if (target.sources.empty() && target.kind != core::TargetKind::InterfaceLibrary) {
            continue;
        }
        file.addStatement(CMakeStatement(
            "add_subdirectory",
            {target.name}
        ));
    }

    return file;
}

// ==========================================================================
// buildDependenciesFile — cmake/dependencies.cmake
//
// For each enabled DependencySpec, emit the appropriate CMake code
// depending on DependencySource:
//
//   FetchContent:       FetchContent_Declare(...) + FetchContent_MakeAvailable(...)
//   CPM:                CPMAddPackage(...)  [requires CPM.cmake to be included]
//   VcpkgFindPackage:   find_package(... CONFIG REQUIRED)
//   SystemFindPackage:  find_package(...)
//
// Why separate this into its own file? The design doc §10 says keeping
// a dedicated dependency file means the root CMakeLists.txt stays focused
// on structure, and dependencies can be updated/re-generated without
// touching the rest of the tree. This matches the pattern used by
// real-world CMake projects (e.g., the "cmake/" directory pattern).
// ==========================================================================

CMakeFile ModernCMakeGenerator::buildDependenciesFile(const core::Project& project) const {
    CMakeFile file;

    // Count how many deps we have per source type for formatting.
    std::vector<const core::DependencySpec*> fetchContentDeps;
    std::vector<const core::DependencySpec*> cpmDeps;
    std::vector<const core::DependencySpec*> findPackageDeps;

    for (const auto& dep : project.dependencies) {
        if (!dep.enabled) continue;
        switch (dep.source) {
            case core::DependencySource::FetchContent:
                fetchContentDeps.push_back(&dep);
                break;
            case core::DependencySource::CPM:
                cpmDeps.push_back(&dep);
                break;
            case core::DependencySource::VcpkgFindPackage:
            case core::DependencySource::SystemFindPackage:
                findPackageDeps.push_back(&dep);
                break;
        }
    }

    // ---- FetchContent dependencies ----
    if (!fetchContentDeps.empty()) {
        file.addComment("FetchContent dependencies");
        // First, declare all FetchContent dependencies.
        for (const auto* dep : fetchContentDeps) {
            // Build the declare call with the dependency name.
            // FetchContent_Declare(<name> GIT_REPOSITORY <url> GIT_TAG <tag>)
            //
            // We add a descriptive comment above each declaration so a
            // human reading the file knows what each dependency is.
            std::vector<std::string> args;
            args.push_back(dep->name);
            if (!dep->gitRepo.empty()) {
                args.push_back("GIT_REPOSITORY");
                args.push_back(dep->gitRepo);
            }
            if (!dep->gitTag.empty()) {
                args.push_back("GIT_TAG");
                args.push_back(dep->gitTag);
            }
            file.addStatement(CMakeStatement(
                "FetchContent_Declare",
                args,
                "Dependency: " + dep->name
            ));
        }
        // Then make them available, wrapped in a single call.
        // Note: FetchContent_MakeAvailable can take multiple arguments,
        // but some older CMake versions (pre-3.30) only support one
        // argument. Our cmake_minimum_required is 3.21, so we need to
        // be safe and make each available individually. However, the
        // design doc says we target 3.21+ which supports multi-arg.
        // We'll use a single call for cleanliness.
        {
            std::vector<std::string> makeAvailableArgs;
            for (const auto* dep : fetchContentDeps) {
                makeAvailableArgs.push_back(dep->name);
            }
            file.addStatement(CMakeStatement(
                "FetchContent_MakeAvailable",
                makeAvailableArgs
            ));
        }
        file.addBlankLine();
    }

    // ---- find_package dependencies (vcpkg, system) ----
    if (!findPackageDeps.empty()) {
        file.addComment("find_package dependencies (vcpkg / system)");
        for (const auto* dep : findPackageDeps) {
            // find_package(<name> CONFIG REQUIRED)
            // The name here is the dependency's link target name rather
            // than the LazyCMake dependency name — for example, a
            // dependency named "fmt" with linkTargets {"fmt::fmt"} should
            // produce find_package(fmt CONFIG REQUIRED). We use the
            // dependency name by default, since that's typically the
            // CMake package name.
            std::vector<std::string> args;
            args.push_back(dep->name);
            args.push_back("CONFIG");
            args.push_back("REQUIRED");

            // For system find_package, we add QUIET to avoid warnings
            // if the package isn't found (the user might have a fallback).
            if (dep->source == core::DependencySource::SystemFindPackage) {
                args.push_back("QUIET");
            }

            file.addStatement(CMakeStatement(
                "find_package",
                args,
                "Dependency: " + dep->name
            ));
        }
        file.addBlankLine();
    }

    // ---- CPM dependencies ----
    // CPM.cmake uses CPMAddPackage which takes the same params as
    // FetchContent_Declare but with a slightly different syntax.
    // We assume CPM.cmake has been included by the project already
    // (LazyCMake itself doesn't use CPM, but a user's project might).
    if (!cpmDeps.empty()) {
        file.addComment("CPM dependencies");
        for (const auto* dep : cpmDeps) {
            // CPMAddPackage("name@tag" GIT_REPOSITORY <url>)
            std::string nameWithTag = dep->name;
            if (!dep->gitTag.empty()) {
                nameWithTag += "@" + dep->gitTag;
            }
            std::vector<std::string> args;
            args.push_back(nameWithTag);
            if (!dep->gitRepo.empty()) {
                args.push_back("GIT_REPOSITORY");
                args.push_back(dep->gitRepo);
            }
            file.addStatement(CMakeStatement(
                "CPMAddPackage",
                args,
                "Dependency: " + dep->name
            ));
        }
        file.addBlankLine();
    }

    // If no dependencies were actually written (shouldn't happen since
    // we only call this when hasEnabledDeps is true), add a note.
    if (file.empty()) {
        file.addComment("No dependencies to declare (all are empty or disabled)");
        file.addBlankLine();
    }

    return file;
}

// ==========================================================================
// buildTargetFile — per-target CMakeLists.txt
//
// Layout:
//   add_library(<name> <STATIC|SHARED|INTERFACE> [sources...])
//   target_include_directories(<name> [PUBLIC|PRIVATE] <dirs...>)
//   target_link_libraries(<name> [PUBLIC|PRIVATE] <targets...>)
//   target_compile_features(<name> PUBLIC <features...>)
//   target_compile_definitions(<name> [PUBLIC|PRIVATE] <definitions...>)
//
// Key rules from §10:
//   - InterfaceLibrary targets: no sources, use INTERFACE keyword.
//   - Executable targets: use add_executable, not add_library.
//   - target_compile_features uses PUBLIC visibility (matching modern
//     CMake conventions: if a target needs C++20, anything linking it
//     probably does too).
//   - target_include_directories uses PUBLIC for the target's public
//     headers (include dirs), PRIVATE for internal-only dirs.
//     For now, all includeDirs are treated as PUBLIC since the model
//     doesn't distinguish between public/private. This is a simplification
//     that could be refined in a later phase.
//   - target_link_libraries: each item in linkTargets is linked PUBLICly.
//     This is the common case for app-level targets; library targets might
//     want PRIVATE linking. Again, a simplification.
//   - target_compile_definitions: each definition in compileDefinitions
//     is emitted as -DNAME=VALUE, PUBLICly.
// ==========================================================================

CMakeFile ModernCMakeGenerator::buildTargetFile(const core::Project& project,
                                                  const core::Target& target) const {
    CMakeFile file;

    // ---- Determine if this target should have a file ----
    // Interface libraries always get a file (they have INTERFACE properties).
    // Other targets with no sources are debatable — we skip them because
    // they have nothing to contribute to the build. The validator already
    // warns about this case (see TargetValidator::checkEmptySources).
    if (target.sources.empty() && target.kind != core::TargetKind::InterfaceLibrary) {
        return file;  // empty file
    }

    // ---- add_library / add_executable ----
    {
        std::vector<std::string> args;
        args.push_back(target.name);
        std::string kindStr = buildTargetKindString(target.kind);

        if (target.kind == core::TargetKind::Executable) {
            // add_executable(<name> [sources...])
            // Executable targets use add_executable, which takes the target
            // name followed by source files (no kind keyword like STATIC).
            // Source paths are relative to the project root; we adjust them
            // to be relative to this target's subdirectory by prepending "..".
            std::vector<std::string> execArgs;
            execArgs.push_back(target.name);
            for (const auto& src : target.sources) {
                std::filesystem::path relativeSrc =
                    std::filesystem::path("..") / src.path;
                execArgs.push_back(relativeSrc.generic_string());
            }
            file.addStatement(CMakeStatement(
                "add_executable", execArgs
            ));
        } else {
            // add_library(<name> <STATIC|SHARED|INTERFACE> [sources...])
            args.push_back(kindStr);
            for (const auto& src : target.sources) {
                // Source paths are relative to the project root, but the
                // CMakeLists.txt lives in a subdirectory. We need to make
                // the paths relative to the CMakeLists.txt location.
                // 
                // Convention: we put the target's CMakeLists.txt in
                // <target_name>/CMakeLists.txt. The source paths in the
                // Project model are relative to the project root. So we
                // need to adjust them to be relative to the target's
                // subdirectory.
                //
                // For example, if the project root has "src/main.cpp" and
                // the target is named "app", the CMakeLists.txt goes at
                // "app/CMakeLists.txt", and the source path should be
                // "../src/main.cpp" to be relative to that directory.
                //
                // However, a cleaner pattern used by many real projects is
                // to keep sources flat relative to the target directory.
                // Since LazyCMake's Project model stores sources relative
                // to the project root, we adjust here.
                std::filesystem::path relativeSrc =
                    std::filesystem::path("..") / src.path;
                args.push_back(relativeSrc.generic_string());
            }
            file.addStatement(CMakeStatement(
                "add_library", args
            ));
        }
        file.addBlankLine();
    }

    // ---- target_include_directories ----
    // We treat all includeDirs as PUBLIC. The Project model doesn't
    // currently distinguish public vs. private include dirs (it could be
    // extended in the future). This matches the design doc's §10.1
    // mandate to always use target-based (not directory-scoped) commands.
    if (!target.includeDirs.empty()) {
        std::vector<std::string> args;
        args.push_back(target.name);
        args.push_back("PUBLIC");
        for (const auto& dir : target.includeDirs) {
            args.push_back(dir.generic_string());
        }
        file.addStatement(CMakeStatement(
            "target_include_directories", args
        ));
    }

    // ---- target_link_libraries ----
    // Link targets can be:
    //   - Other Target names within the project (e.g., "engine").
    //   - Dependency link names (e.g., "fmt::fmt").
    // We link them all as PUBLIC. This is conservative and correct for
    // executable targets; library targets might want PRIVATE linking,
    // but that's a refinement for a future phase.
    if (!target.linkTargets.empty()) {
        std::vector<std::string> args;
        args.push_back(target.name);
        args.push_back("PUBLIC");
        for (const auto& link : target.linkTargets) {
            args.push_back(link);
        }
        file.addStatement(CMakeStatement(
            "target_link_libraries", args
        ));
    }

    // ---- target_compile_features ----
    // Emit the C++ or C standard as a compile feature.
    // target_compile_features(<name> PUBLIC <feature>)
    // e.g., target_compile_features(app PUBLIC cxx_std_20)
    auto cppStd = effectiveCppStandard(project, target);
    auto cStd = effectiveCStandard(project, target);
    if (cppStd.has_value() || cStd.has_value()) {
        std::vector<std::string> args;
        args.push_back(target.name);
        args.push_back("PUBLIC");
        if (cppStd.has_value()) {
            args.push_back(core::toCMakeFeatureString(*cppStd));
        }
        if (cStd.has_value()) {
            args.push_back(core::toCMakeFeatureString(*cStd));
        }
        if (args.size() > 2) {  // has actual features beyond name + PUBLIC
            file.addStatement(CMakeStatement(
                "target_compile_features", args
            ));
        }
    }

    // ---- target_compile_definitions ----
    // Each entry in compileDefinitions becomes a PUBLIC definition.
    // target_compile_definitions(<name> PUBLIC <defines...>)
    // e.g., target_compile_definitions(engine PUBLIC ENGINE_BUILD=1)
    if (!target.compileDefinitions.empty()) {
        std::vector<std::string> args;
        args.push_back(target.name);
        args.push_back("PUBLIC");
        for (const auto& [key, value] : target.compileDefinitions) {
            if (value.empty()) {
                args.push_back(key);
            } else {
                args.push_back(key + "=" + value);
            }
        }
        file.addStatement(CMakeStatement(
            "target_compile_definitions", args
        ));
    }

    return file;
}

// ==========================================================================
// Helper implementations
// ==========================================================================

std::string ModernCMakeGenerator::buildLanguageArg(const core::Project& project) const {
    // CMake's project() command expects the language argument as:
    //   C          — for C-only projects
    //   CXX        — for C++-only projects
    //   C CXX      — for mixed-language projects
    switch (project.language) {
        case core::Language::C:
            return "C";
        case core::Language::Cpp:
            return "CXX";
        case core::Language::Both:
            return "C CXX";
    }
    return "CXX";  // fallback (shouldn't reach here)
}

std::optional<core::CppStandard> ModernCMakeGenerator::effectiveCppStandard(
    const core::Project& project, const core::Target& target) const {
    // Target-level override takes precedence over project-level default.
    // This matches the design doc's §6 model where Target::cppStandard
    // "overrides Project default if set."
    if (target.cppStandard.has_value()) {
        return target.cppStandard;
    }
    return project.cppStandard;
}

std::optional<core::CStandard> ModernCMakeGenerator::effectiveCStandard(
    const core::Project& project, const core::Target& /*target*/) const {
    // Stand-in for future C standard support. Currently, the Project model
    // only has cStandard, and targets don't have a C standard override.
    // When target-level C standard is added, this will follow the same
    // pattern as effectiveCppStandard.
    return project.cStandard;
}

std::string ModernCMakeGenerator::buildTargetKindString(core::TargetKind kind) const {
    switch (kind) {
        case core::TargetKind::StaticLibrary:
            return "STATIC";
        case core::TargetKind::SharedLibrary:
            return "SHARED";
        case core::TargetKind::InterfaceLibrary:
            return "INTERFACE";
        case core::TargetKind::Executable:
            return "";  // Executable is handled by add_executable, not add_library
    }
    return "STATIC";  // fallback
}

} // namespace lazycmake::cmake_gen
