#include <catch2/catch_test_macros.hpp>

#include "lazycmake/core/target_validator.hpp"

using namespace lazycmake::core;

namespace {

bool hasIssueForTarget(const std::vector<ValidationIssue>& issues,
                        const std::string& targetName,
                        ValidationSeverity severity) {
    for (const auto& issue : issues) {
        if (issue.targetName == targetName && issue.severity == severity) return true;
    }
    return false;
}

}  // namespace

TEST_CASE("A valid single-target project has no issues", "[target_validator]") {
    Project project;
    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "src/main.cpp"});
    project.targets.push_back(app);

    auto issues = TargetValidator::validate(project);
    CHECK(issues.empty());
}

TEST_CASE("Duplicate target names are flagged as an error", "[target_validator]") {
    Project project;
    project.targets.push_back(Target{.name = "app", .kind = TargetKind::Executable,
                                       .sources = {SourceFileRef{.path = "a.cpp"}}});
    project.targets.push_back(Target{.name = "app", .kind = TargetKind::Executable,
                                       .sources = {SourceFileRef{.path = "b.cpp"}}});

    auto issues = TargetValidator::validate(project);
    CHECK(hasIssueForTarget(issues, "app", ValidationSeverity::Error));
}

TEST_CASE("A target linking against itself is rejected", "[target_validator]") {
    Project project;
    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "main.cpp"});
    app.linkTargets.push_back("app");
    project.targets.push_back(app);

    auto issues = TargetValidator::validate(project);
    CHECK(hasIssueForTarget(issues, "app", ValidationSeverity::Error));
}

TEST_CASE("Linking against an unknown name produces a warning, not an error",
          "[target_validator]") {
    Project project;
    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "main.cpp"});
    app.linkTargets.push_back("totally_unknown_thing");
    project.targets.push_back(app);

    auto issues = TargetValidator::validate(project);
    CHECK(hasIssueForTarget(issues, "app", ValidationSeverity::Warning));
    CHECK_FALSE(hasIssueForTarget(issues, "app", ValidationSeverity::Error));
}

TEST_CASE("Linking against an enabled dependency's link name is valid", "[target_validator]") {
    Project project;
    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "main.cpp"});
    app.linkTargets.push_back("fmt::fmt");
    project.targets.push_back(app);

    DependencySpec fmtDep;
    fmtDep.name = "fmt";
    fmtDep.linkTargets = {"fmt::fmt"};
    fmtDep.enabled = true;
    project.dependencies.push_back(fmtDep);

    auto issues = TargetValidator::validate(project);
    CHECK(issues.empty());
}

TEST_CASE("Linking against a disabled dependency's link name still warns",
          "[target_validator]") {
    Project project;
    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "main.cpp"});
    app.linkTargets.push_back("fmt::fmt");
    project.targets.push_back(app);

    DependencySpec fmtDep;
    fmtDep.name = "fmt";
    fmtDep.linkTargets = {"fmt::fmt"};
    fmtDep.enabled = false;  // not enabled
    project.dependencies.push_back(fmtDep);

    auto issues = TargetValidator::validate(project);
    CHECK(hasIssueForTarget(issues, "app", ValidationSeverity::Warning));
}

TEST_CASE("A direct two-target link cycle is detected", "[target_validator]") {
    Project project;
    project.targets.push_back(Target{.name = "a", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "a.cpp"}},
                                       .linkTargets = {"b"}});
    project.targets.push_back(Target{.name = "b", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "b.cpp"}},
                                       .linkTargets = {"a"}});

    auto issues = TargetValidator::validate(project);
    bool foundCycleError = false;
    for (const auto& issue : issues) {
        if (issue.severity == ValidationSeverity::Error &&
            issue.message.find("Cyclic") != std::string::npos) {
            foundCycleError = true;
        }
    }
    CHECK(foundCycleError);
}

TEST_CASE("A longer three-target link cycle is detected", "[target_validator]") {
    Project project;
    project.targets.push_back(Target{.name = "a", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "a.cpp"}},
                                       .linkTargets = {"b"}});
    project.targets.push_back(Target{.name = "b", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "b.cpp"}},
                                       .linkTargets = {"c"}});
    project.targets.push_back(Target{.name = "c", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "c.cpp"}},
                                       .linkTargets = {"a"}});

    auto issues = TargetValidator::validate(project);
    bool foundCycleError = false;
    for (const auto& issue : issues) {
        if (issue.severity == ValidationSeverity::Error &&
            issue.message.find("Cyclic") != std::string::npos) {
            foundCycleError = true;
        }
    }
    CHECK(foundCycleError);
}

TEST_CASE("A valid diamond dependency graph produces no cycle error", "[target_validator]") {
    // app -> {engine, renderer}, engine -> core, renderer -> core
    Project project;
    project.targets.push_back(Target{.name = "core", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "core.cpp"}}});
    project.targets.push_back(Target{.name = "engine", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "engine.cpp"}},
                                       .linkTargets = {"core"}});
    project.targets.push_back(Target{.name = "renderer", .kind = TargetKind::StaticLibrary,
                                       .sources = {SourceFileRef{.path = "renderer.cpp"}},
                                       .linkTargets = {"core"}});
    project.targets.push_back(Target{.name = "app", .kind = TargetKind::Executable,
                                       .sources = {SourceFileRef{.path = "main.cpp"}},
                                       .linkTargets = {"engine", "renderer"}});

    auto issues = TargetValidator::validate(project);
    for (const auto& issue : issues) {
        CHECK(issue.message.find("Cyclic") == std::string::npos);
    }
}

TEST_CASE("A target with no sources warns unless it is an interface library",
          "[target_validator]") {
    Project project;
    project.targets.push_back(Target{.name = "empty_lib", .kind = TargetKind::StaticLibrary});

    auto issues = TargetValidator::validate(project);
    CHECK(hasIssueForTarget(issues, "empty_lib", ValidationSeverity::Warning));
}

TEST_CASE("An interface library with no sources does not warn", "[target_validator]") {
    Project project;
    project.targets.push_back(Target{.name = "header_only", .kind = TargetKind::InterfaceLibrary});

    auto issues = TargetValidator::validate(project);
    CHECK_FALSE(hasIssueForTarget(issues, "header_only", ValidationSeverity::Warning));
}
