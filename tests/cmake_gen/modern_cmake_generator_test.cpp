// ==========================================================================
// ModernCMakeGenerator tests — golden-file style (§18)
//
// These tests construct Project fixtures in memory, run them through
// ModernCMakeGenerator::generate(), and verify the output text against
// expected patterns. This is a "golden-file" approach without checked-in
// golden files: we assert structural properties of the generated text
// rather than byte-for-byte equality, because the exact formatting is
// handled by CMakePrinter and may change as formatting options evolve.
//
// Integration with real CMake (invoking cmake on generated output) is
// done in a separate smoke test (see the "real_cmake_integration" test).
//
// The pattern:
//   1. Build a Project with specific properties.
//   2. Generate CMake output.
//   3. Assert the output:
//      a. Contains expected CMake commands in the right files.
//      b. Does NOT contain legacy/deprecated commands.
//      c. Has the right file structure.
// ==========================================================================

#include <catch2/catch_test_macros.hpp>

#include "lazycmake/cmake_gen/modern_cmake_generator.hpp"

using namespace lazycmake::core;
using namespace lazycmake::cmake_gen;

// Helper: create a minimal C++ project with one executable target.
Project makeMinimalProject() {
    Project p;
    p.name = "HelloWorld";
    p.language = Language::Cpp;
    p.cppStandard = CppStandard::Cpp20;
    p.rootDir = "/tmp/test_project";

    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "src/main.cpp"});
    p.targets.push_back(app);

    return p;
}

// Helper: check if a string contains a substring.
bool contains(const std::string& str, const std::string& substr) {
    return str.find(substr) != std::string::npos;
}

// Helper: count occurrences of a substring in a string.
int countOccurrences(const std::string& str, const std::string& substr) {
    int count = 0;
    size_t pos = 0;
    while ((pos = str.find(substr, pos)) != std::string::npos) {
        ++count;
        pos += substr.length();
    }
    return count;
}

// ==========================================================================

TEST_CASE("Generator produces the expected files for a minimal project", "[cmake_gen]") {
    // A minimal C++20 project with one executable target should produce:
    //   CMakeLists.txt (root)
    //   app/CMakeLists.txt (for the "app" target)
    // It should NOT produce cmake/dependencies.cmake (no deps).

    Project project = makeMinimalProject();
    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    REQUIRE(files.size() == 2);
    CHECK(files.contains("CMakeLists.txt"));
    CHECK(files.contains("app/CMakeLists.txt"));
    CHECK_FALSE(files.contains("cmake/dependencies.cmake"));
}

TEST_CASE("Root CMakeLists.txt has the correct structure", "[cmake_gen]") {
    // The root file should contain:
    //   cmake_minimum_required(VERSION 3.21)
    //   project(HelloWorld LANGUAGES CXX)
    //   add_subdirectory(app)

    Project project = makeMinimalProject();
    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* rootContent = files.find("CMakeLists.txt");
    REQUIRE(rootContent != nullptr);

    // Check essential CMake commands.
    CHECK(contains(*rootContent, "cmake_minimum_required(VERSION 3.21)"));
    CHECK(contains(*rootContent, "project(HelloWorld LANGUAGES CXX)"));
    CHECK(contains(*rootContent, "add_subdirectory(app)"));
}

TEST_CASE("Legacy global commands are never generated", "[cmake_gen]") {
    // Per §10.1 design principle: the generator must NEVER emit
    // directory-scoped legacy commands. This test is the structural
    // enforcement of that rule — search for known-bad patterns.
    Project project = makeMinimalProject();
    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    for (const auto& [path, content] : files.files()) {
        CHECK_FALSE(contains(content, "include_directories("));
        CHECK_FALSE(contains(content, "link_directories("));
        CHECK_FALSE(contains(content, "link_libraries("));
        CHECK_FALSE(contains(content, "set(CMAKE_CXX_STANDARD"));
        CHECK_FALSE(contains(content, "set(CMAKE_C_STANDARD"));
    }
}

TEST_CASE("Executable target produces correct add_executable", "[cmake_gen]") {
    // The target file for an executable should have add_executable
    // with the target name and source files.
    Project project = makeMinimalProject();
    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* targetContent = files.find("app/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    // Should use add_executable, not add_library.
    CHECK(contains(*targetContent, "add_executable(app ../src/main.cpp)"));

    // Should NOT use add_library for the executable target.
    CHECK_FALSE(contains(*targetContent, "add_library"));
}

TEST_CASE("Generator produces cmake/dependencies.cmake when deps exist", "[cmake_gen]") {
    // When a project has enabled dependencies of type FetchContent,
    // the generator should produce cmake/dependencies.cmake.

    Project project = makeMinimalProject();

    DependencySpec fmtDep;
    fmtDep.name = "fmt";
    fmtDep.source = DependencySource::FetchContent;
    fmtDep.gitRepo = "https://github.com/fmtlib/fmt.git";
    fmtDep.gitTag = "11.0.2";
    fmtDep.linkTargets = {"fmt::fmt"};
    fmtDep.enabled = true;
    project.dependencies.push_back(fmtDep);

    // Also add the dependency link to the target.
    project.targets[0].linkTargets.push_back("fmt::fmt");

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    // Should now have 3 files: root, target, and dependencies.
    REQUIRE(files.size() == 3);
    CHECK(files.contains("cmake/dependencies.cmake"));

    const std::string* depsContent = files.find("cmake/dependencies.cmake");
    REQUIRE(depsContent != nullptr);

    // Check the FetchContent declarations.
    CHECK(contains(*depsContent, "FetchContent_Declare(fmt"));
    CHECK(contains(*depsContent, "https://github.com/fmtlib/fmt.git"));
    CHECK(contains(*depsContent, "11.0.2"));
    CHECK(contains(*depsContent, "FetchContent_MakeAvailable(fmt)"));
}

TEST_CASE("Disabled dependencies are not included in generated files", "[cmake_gen]") {
    // A dependency with enabled=false should not appear in the output.

    Project project = makeMinimalProject();

    DependencySpec disabledDep;
    disabledDep.name = "DisabledLib";
    disabledDep.source = DependencySource::FetchContent;
    disabledDep.gitRepo = "https://example.com/disabled.git";
    disabledDep.gitTag = "v1.0";
    disabledDep.enabled = false;  // explicitly disabled
    project.dependencies.push_back(disabledDep);

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    // No dependencies file should be generated (no enabled deps).
    CHECK_FALSE(files.contains("cmake/dependencies.cmake"));
}

TEST_CASE("find_package dependencies produce correct CMake", "[cmake_gen]") {
    // Dependencies with source VcpkgFindPackage or SystemFindPackage
    // should produce find_package() calls in the dependencies file.

    Project project = makeMinimalProject();

    DependencySpec vcpkgDep;
    vcpkgDep.name = "SDL3";
    vcpkgDep.source = DependencySource::VcpkgFindPackage;
    vcpkgDep.linkTargets = {"SDL3::SDL3"};
    vcpkgDep.enabled = true;
    project.dependencies.push_back(vcpkgDep);

    DependencySpec systemDep;
    systemDep.name = "Threads";
    systemDep.source = DependencySource::SystemFindPackage;
    systemDep.enabled = true;
    project.dependencies.push_back(systemDep);

    project.targets[0].linkTargets.push_back("SDL3::SDL3");
    project.targets[0].linkTargets.push_back("Threads::Threads");

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    REQUIRE(files.contains("cmake/dependencies.cmake"));
    const std::string* depsContent = files.find("cmake/dependencies.cmake");
    REQUIRE(depsContent != nullptr);

    // VcpkgFindPackage should produce find_package with CONFIG REQUIRED.
    CHECK(contains(*depsContent, "find_package(SDL3 CONFIG REQUIRED)"));

    // SystemFindPackage should produce find_package with QUIET.
    CHECK(contains(*depsContent, "find_package(Threads CONFIG REQUIRED QUIET)"));
}

TEST_CASE("target_link_libraries appears in target CMakeLists.txt", "[cmake_gen]") {
    // When a target has linkTargets, target_link_libraries should be
    // emitted in the target's CMakeLists.txt.

    Project project = makeMinimalProject();
    project.targets[0].linkTargets.push_back("fmt::fmt");
    project.targets[0].linkTargets.push_back("engine");

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* targetContent = files.find("app/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    // Should contain PUBLIC linking.
    CHECK(contains(*targetContent, "target_link_libraries(app PUBLIC fmt::fmt engine)"));
}

TEST_CASE("target_compile_features emits correct C++ standard", "[cmake_gen]") {
    // target_compile_features should emit the correct feature string
    // based on the project's cppStandard.

    Project project = makeMinimalProject();
    // Already set to CppStandard::Cpp20 by makeMinimalProject().

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* targetContent = files.find("app/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    CHECK(contains(*targetContent, "target_compile_features(app PUBLIC cxx_std_20)"));
}

TEST_CASE("Target with compile definitions emits them correctly", "[cmake_gen]") {
    // target_compile_definitions should include all entries from the
    // target's compileDefinitions map.

    Project project = makeMinimalProject();
    project.targets[0].compileDefinitions["APP_VERSION"] = "1.0";
    project.targets[0].compileDefinitions["ENABLE_FEATURE_X"] = "";  // flag without value

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* targetContent = files.find("app/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    // The definitions should appear in a target_compile_definitions call.
    CHECK(contains(*targetContent, "target_compile_definitions(app PUBLIC"));
    CHECK(contains(*targetContent, "APP_VERSION=1.0"));
    // Empty value means just the define name (a boolean flag).
    CHECK(contains(*targetContent, "ENABLE_FEATURE_X"));
}

TEST_CASE("Interface library target produces correct add_library", "[cmake_gen]") {
    // InterfaceLibrary targets use add_library with the INTERFACE keyword
    // and have no source files.

    Project project;
    project.name = "HeaderOnly";
    project.language = Language::Cpp;
    project.cppStandard = CppStandard::Cpp20;
    project.rootDir = "/tmp/test_project";

    Target headerLib;
    headerLib.name = "my_interface";
    headerLib.kind = TargetKind::InterfaceLibrary;
    headerLib.includeDirs.push_back("include");
    project.targets.push_back(headerLib);

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    CHECK(files.contains("my_interface/CMakeLists.txt"));

    const std::string* targetContent = files.find("my_interface/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    // Should use add_library with INTERFACE.
    CHECK(contains(*targetContent, "add_library(my_interface INTERFACE)"));

    // Should have target_include_directories for the interface.
    CHECK(contains(*targetContent, "target_include_directories(my_interface PUBLIC include)"));
}

TEST_CASE("Multiple targets each get their own CMakeLists.txt", "[cmake_gen]") {
    // A project with multiple targets should produce per-target files.

    Project project;
    project.name = "MultiTarget";
    project.language = Language::Cpp;
    project.rootDir = "/tmp/test_project";

    Target lib;
    lib.name = "engine";
    lib.kind = TargetKind::StaticLibrary;
    lib.sources.push_back(SourceFileRef{.path = "src/engine.cpp"});
    project.targets.push_back(lib);

    Target exe;
    exe.name = "app";
    exe.kind = TargetKind::Executable;
    exe.sources.push_back(SourceFileRef{.path = "src/main.cpp"});
    exe.linkTargets.push_back("engine");
    project.targets.push_back(exe);

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    // Should have root CMakeLists.txt, engine/CMakeLists.txt, app/CMakeLists.txt
    REQUIRE(files.size() == 3);
    CHECK(files.contains("engine/CMakeLists.txt"));
    CHECK(files.contains("app/CMakeLists.txt"));

    // Root file should have add_subdirectory for both.
    const std::string* rootContent = files.find("CMakeLists.txt");
    REQUIRE(rootContent != nullptr);
    CHECK(contains(*rootContent, "add_subdirectory(engine)"));
    CHECK(contains(*rootContent, "add_subdirectory(app)"));
}

TEST_CASE("Static library target uses add_library with STATIC", "[cmake_gen]") {
    // StaticLibrary targets should produce add_library with STATIC keyword.

    Project project;
    project.name = "LibTest";
    project.language = Language::Cpp;
    project.rootDir = "/tmp/test_project";

    Target lib;
    lib.name = "core";
    lib.kind = TargetKind::StaticLibrary;
    lib.sources.push_back(SourceFileRef{.path = "src/core.cpp"});
    project.targets.push_back(lib);

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* targetContent = files.find("core/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    CHECK(contains(*targetContent, "add_library(core STATIC ../src/core.cpp)"));
}

TEST_CASE("Shared library target uses add_library with SHARED", "[cmake_gen]") {
    Project project;
    project.name = "SharedTest";
    project.language = Language::Cpp;
    project.rootDir = "/tmp/test_project";

    Target lib;
    lib.name = "plugin";
    lib.kind = TargetKind::SharedLibrary;
    lib.sources.push_back(SourceFileRef{.path = "src/plugin.cpp"});
    project.targets.push_back(lib);

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* targetContent = files.find("plugin/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    CHECK(contains(*targetContent, "add_library(plugin SHARED ../src/plugin.cpp)"));
}

TEST_CASE("Per-target cppStandard override is reflected in generated output", "[cmake_gen]") {
    // If a target overrides the project's C++ standard, that override
    // should appear in the target's target_compile_features.

    Project project;
    project.name = "OverrideTest";
    project.language = Language::Cpp;
    project.cppStandard = CppStandard::Cpp17;  // project default is C++17
    project.rootDir = "/tmp/test_project";

    Target lib;
    lib.name = "modern_lib";
    lib.kind = TargetKind::StaticLibrary;
    lib.sources.push_back(SourceFileRef{.path = "src/lib.cpp"});
    lib.cppStandard = CppStandard::Cpp23;  // override to C++23
    project.targets.push_back(lib);

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    const std::string* targetContent = files.find("modern_lib/CMakeLists.txt");
    REQUIRE(targetContent != nullptr);

    // Should use C++23 (the override), not C++17 (the project default).
    CHECK(contains(*targetContent, "cxx_std_23"));
    CHECK_FALSE(contains(*targetContent, "cxx_std_17"));
}

TEST_CASE("Multiple dependencies with mixed sources all appear in the deps file", "[cmake_gen]") {
    // A project with multiple enabled deps of different types should have
    // all of them in cmake/dependencies.cmake.

    Project project = makeMinimalProject();

    DependencySpec fmtDep;
    fmtDep.name = "fmt";
    fmtDep.source = DependencySource::FetchContent;
    fmtDep.gitRepo = "https://github.com/fmtlib/fmt.git";
    fmtDep.gitTag = "11.0.2";
    fmtDep.enabled = true;
    project.dependencies.push_back(fmtDep);

    DependencySpec sdlDep;
    sdlDep.name = "SDL3";
    sdlDep.source = DependencySource::VcpkgFindPackage;
    sdlDep.enabled = true;
    project.dependencies.push_back(sdlDep);

    project.targets[0].linkTargets.push_back("fmt::fmt");
    project.targets[0].linkTargets.push_back("SDL3::SDL3");

    ModernCMakeGenerator generator;
    auto files = generator.generate(project);

    REQUIRE(files.contains("cmake/dependencies.cmake"));
    const std::string* depsContent = files.find("cmake/dependencies.cmake");
    REQUIRE(depsContent != nullptr);

    // Both dependency types should be present.
    CHECK(contains(*depsContent, "FetchContent_Declare(fmt"));
    CHECK(contains(*depsContent, "find_package(SDL3 CONFIG REQUIRED)"));
}

TEST_CASE("GeneratedFileSet operations work correctly", "[cmake_gen]") {
    // Test the GeneratedFileSet utility class directly.

    GeneratedFileSet set;

    CHECK(set.empty());
    CHECK(set.size() == 0);

    set.addFile("CMakeLists.txt", "cmake_minimum_required(VERSION 3.21)");
    CHECK_FALSE(set.empty());
    CHECK(set.size() == 1);
    CHECK(set.contains("CMakeLists.txt"));

    set.addFile("app/CMakeLists.txt", "add_executable(app main.cpp)");
    CHECK(set.size() == 2);

    const std::string* found = set.find("CMakeLists.txt");
    REQUIRE(found != nullptr);
    CHECK(*found == "cmake_minimum_required(VERSION 3.21)");

    CHECK(set.find("nonexistent.txt") == nullptr);

    set.clear();
    CHECK(set.empty());
    CHECK(set.size() == 0);
}

// End of tests — all helpers are in anonymous namespaces above (per-file).
