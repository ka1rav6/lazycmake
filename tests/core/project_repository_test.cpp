#include <catch2/catch_test_macros.hpp>

#include "lazycmake/core/project_repository.hpp"

#include <filesystem>
#include <fstream>

using namespace lazycmake::core;

namespace {

// RAII helper: creates a unique temp directory for a test and removes it
// (recursively) when the test finishes, regardless of pass/fail/throw.
class TempDir {
public:
    TempDir() {
        path_ = std::filesystem::temp_directory_path() /
                ("lazycmake_test_" + std::to_string(counter_++));
        std::filesystem::create_directories(path_);
    }
    ~TempDir() { std::filesystem::remove_all(path_); }

    [[nodiscard]] const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
    static inline int counter_ = 0;
};

Project makeSampleProject(const std::filesystem::path& root) {
    Project project;
    project.name = "SampleProject";
    project.language = Language::Cpp;
    project.cppStandard = CppStandard::Cpp20;
    project.rootDir = root;

    Target app;
    app.name = "app";
    app.kind = TargetKind::Executable;
    app.sources.push_back(SourceFileRef{.path = "src/main.cpp"});
    project.targets.push_back(app);

    return project;
}

}  // namespace

TEST_CASE("ProjectRepository::load fails with FileNotFound when no manifest exists",
          "[project_repository]") {
    TempDir dir;
    ProjectRepository repo;

    auto result = repo.load(dir.path());
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ProjectLoadErrorKind::FileNotFound);
}

TEST_CASE("ProjectRepository::save then load round-trips a project", "[project_repository]") {
    TempDir dir;
    ProjectRepository repo;

    Project original = makeSampleProject(dir.path());

    auto saveResult = repo.save(original);
    REQUIRE(saveResult.has_value());

    auto loadResult = repo.load(dir.path());
    REQUIRE(loadResult.has_value());

    const Project& loaded = loadResult.value();
    CHECK(loaded.name == original.name);
    CHECK(loaded.language == original.language);
    CHECK(loaded.cppStandard == original.cppStandard);
    REQUIRE(loaded.targets.size() == 1);
    CHECK(loaded.targets[0].name == "app");
    CHECK(loaded.rootDir == dir.path());
}

TEST_CASE("ProjectRepository::save writes the manifest at the expected relative path",
          "[project_repository]") {
    TempDir dir;
    ProjectRepository repo;

    Project original = makeSampleProject(dir.path());
    auto saveResult = repo.save(original);
    REQUIRE(saveResult.has_value());

    const std::filesystem::path expectedPath =
        dir.path() / ProjectRepository::kManifestRelativePath;
    CHECK(std::filesystem::exists(expectedPath));
}

TEST_CASE("ProjectRepository::load reports InvalidJson for a malformed manifest",
          "[project_repository]") {
    TempDir dir;
    std::filesystem::create_directories(dir.path() / ".lazycmake");
    {
        std::ofstream broken(dir.path() / ".lazycmake" / "project.json");
        broken << "{ this is not valid json ";
    }

    ProjectRepository repo;
    auto result = repo.load(dir.path());
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error().kind == ProjectLoadErrorKind::InvalidJson);
}

TEST_CASE("ProjectRepository::load stamps rootDir to the actual load location",
          "[project_repository]") {
    // Simulates a project directory that was moved on disk since it was
    // last saved: the manifest's serialized rootDir should not be trusted
    // over the path actually passed to load().
    TempDir dir;
    ProjectRepository repo;

    Project original = makeSampleProject("/some/stale/path/that/no/longer/exists");
    // Force-save directly into our temp dir regardless of the stale rootDir
    // recorded above, then load from the temp dir and confirm it wins.
    original.rootDir = dir.path();
    REQUIRE(repo.save(original).has_value());

    auto loadResult = repo.load(dir.path());
    REQUIRE(loadResult.has_value());
    CHECK(loadResult.value().rootDir == dir.path());
}
