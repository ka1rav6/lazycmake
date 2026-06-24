#include <catch2/catch_test_macros.hpp>

#include "lazycmake/core/project.hpp"

using namespace lazycmake::core;

TEST_CASE("Project::findTarget locates an existing target", "[project]") {
    Project project;
    project.targets.push_back(Target{.name = "engine", .kind = TargetKind::StaticLibrary});
    project.targets.push_back(Target{.name = "app", .kind = TargetKind::Executable});

    const Target* found = project.findTarget("engine");
    REQUIRE(found != nullptr);
    CHECK(found->name == "engine");
    CHECK(found->kind == TargetKind::StaticLibrary);
}

TEST_CASE("Project::findTarget returns nullptr for an unknown name", "[project]") {
    Project project;
    project.targets.push_back(Target{.name = "engine", .kind = TargetKind::StaticLibrary});

    CHECK(project.findTarget("nonexistent") == nullptr);
}

TEST_CASE("Project::hasTargetNamed reflects target presence", "[project]") {
    Project project;
    project.targets.push_back(Target{.name = "app", .kind = TargetKind::Executable});

    CHECK(project.hasTargetNamed("app"));
    CHECK_FALSE(project.hasTargetNamed("missing"));
}

TEST_CASE("Project round-trips through JSON with all fields populated", "[project][json]") {
    Project original;
    original.name = "MyGame";
    original.language = Language::Cpp;
    original.cppStandard = CppStandard::Cpp20;
    original.cStandard = std::nullopt;
    original.rootDir = "/home/dev/my-game";
    original.schemaVersion = "1.0";

    Target engine;
    engine.name = "engine";
    engine.kind = TargetKind::StaticLibrary;
    engine.sources.push_back(SourceFileRef{.path = "src/Scene.cpp", .autoDetected = false});
    engine.sources.push_back(SourceFileRef{.path = "src/Renderer.cpp", .autoDetected = true});
    engine.includeDirs.push_back("include");
    engine.linkTargets.push_back("fmt::fmt");
    engine.cppStandard = CppStandard::Cpp20;
    engine.compileDefinitions["ENGINE_BUILD"] = "1";
    original.targets.push_back(engine);

    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "src/main.cpp", .autoDetected = false});
    app.linkTargets.push_back("engine");
    original.targets.push_back(app);

    DependencySpec fmtDep;
    fmtDep.name = "fmt";
    fmtDep.source = DependencySource::FetchContent;
    fmtDep.gitRepo = "https://github.com/fmtlib/fmt.git";
    fmtDep.gitTag = "11.0.2";
    fmtDep.linkTargets = {"fmt::fmt"};
    fmtDep.enabled = true;
    original.dependencies.push_back(fmtDep);

    original.buildConfig.generator = GeneratorKind::Ninja;
    original.buildConfig.defaultBuildType = BuildType::Debug;
    original.buildConfig.buildDir = "build";

    nlohmann::json j = original;
    Project roundTripped = j.get<Project>();

    // rootDir is intentionally excluded from this equality check: in
    // practice ProjectRepository overwrites rootDir with the actual load
    // location rather than trusting the serialized value (see
    // project_repository.cpp), but plain to_json/from_json round-tripping
    // (exercised directly here) should still preserve it byte-for-byte.
    CHECK(roundTripped == original);
}

TEST_CASE("Project with no optional fields set round-trips correctly", "[project][json]") {
    Project original;
    original.name = "Minimal";
    original.language = Language::C;
    original.cppStandard = std::nullopt;
    original.cStandard = CStandard::C17;
    original.rootDir = "/tmp/minimal";

    nlohmann::json j = original;
    Project roundTripped = j.get<Project>();

    CHECK(roundTripped == original);
    CHECK_FALSE(roundTripped.cppStandard.has_value());
    REQUIRE(roundTripped.cStandard.has_value());
    CHECK(*roundTripped.cStandard == CStandard::C17);
}

TEST_CASE("toString/fromString enum helpers round-trip for every enumerator", "[types]") {
    CHECK(languageFromString(toString(Language::C)) == Language::C);
    CHECK(languageFromString(toString(Language::Cpp)) == Language::Cpp);
    CHECK(languageFromString(toString(Language::Both)) == Language::Both);

    CHECK(cppStandardFromString(toString(CppStandard::Cpp17)) == CppStandard::Cpp17);
    CHECK(cppStandardFromString(toString(CppStandard::Cpp20)) == CppStandard::Cpp20);
    CHECK(cppStandardFromString(toString(CppStandard::Cpp23)) == CppStandard::Cpp23);

    CHECK(targetKindFromString(toString(TargetKind::Executable)) == TargetKind::Executable);
    CHECK(targetKindFromString(toString(TargetKind::StaticLibrary)) == TargetKind::StaticLibrary);
    CHECK(targetKindFromString(toString(TargetKind::SharedLibrary)) == TargetKind::SharedLibrary);
    CHECK(targetKindFromString(toString(TargetKind::InterfaceLibrary)) ==
          TargetKind::InterfaceLibrary);

    CHECK(toCMakeFeatureString(CppStandard::Cpp17) == "cxx_std_17");
    CHECK(toCMakeFeatureString(CppStandard::Cpp20) == "cxx_std_20");
    CHECK(toCMakeFeatureString(CppStandard::Cpp23) == "cxx_std_23");
}
