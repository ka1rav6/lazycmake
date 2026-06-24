#include "lazycmake/core/project_repository.hpp"

#include <fstream>
#include <sstream>

namespace lazycmake::core {

std::filesystem::path ProjectRepository::manifestPath(const std::filesystem::path& projectRoot) {
    return projectRoot / kManifestRelativePath;
}

Result<Project, ProjectLoadError> ProjectRepository::load(
    const std::filesystem::path& projectRoot) const {
    const std::filesystem::path path = manifestPath(projectRoot);

    if (!std::filesystem::exists(path)) {
        return Result<Project, ProjectLoadError>::err(ProjectLoadError{
            .kind = ProjectLoadErrorKind::FileNotFound,
            .message = "No project manifest found at " + path.string(),
        });
    }

    std::ifstream file(path);
    if (!file.is_open()) {
        return Result<Project, ProjectLoadError>::err(ProjectLoadError{
            .kind = ProjectLoadErrorKind::FileNotFound,
            .message = "Could not open project manifest at " + path.string(),
        });
    }

    nlohmann::json j;
    try {
        file >> j;
    } catch (const nlohmann::json::parse_error& e) {
        return Result<Project, ProjectLoadError>::err(ProjectLoadError{
            .kind = ProjectLoadErrorKind::InvalidJson,
            .message = std::string("Failed to parse project manifest: ") + e.what(),
        });
    }

    if (!j.contains("schemaVersion")) {
        return Result<Project, ProjectLoadError>::err(ProjectLoadError{
            .kind = ProjectLoadErrorKind::MissingRequiredField,
            .message = "Project manifest is missing required field 'schemaVersion'",
        });
    }

    Project project;
    try {
        project = j.get<Project>();
    } catch (const nlohmann::json::exception& e) {
        return Result<Project, ProjectLoadError>::err(ProjectLoadError{
            .kind = ProjectLoadErrorKind::MissingRequiredField,
            .message = std::string("Project manifest is missing or has malformed fields: ") +
                       e.what(),
        });
    }

    // rootDir on disk is stored relative-friendly but we always want the
    // in-memory model to reflect where it was actually loaded from, in
    // case the project directory was moved since it was last saved.
    project.rootDir = projectRoot;

    project = migrate(std::move(project));

    return Result<Project, ProjectLoadError>::ok(std::move(project));
}

Result<Unit, ProjectSaveError> ProjectRepository::save(const Project& project) const {
    const std::filesystem::path path = manifestPath(project.rootDir);

    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return Result<Unit, ProjectSaveError>::err(ProjectSaveError{
            .kind = ProjectSaveErrorKind::DirectoryNotWritable,
            .message = "Could not create directory " + path.parent_path().string() + ": " +
                       ec.message(),
        });
    }

    nlohmann::json j;
    try {
        j = project;
    } catch (const nlohmann::json::exception& e) {
        return Result<Unit, ProjectSaveError>::err(ProjectSaveError{
            .kind = ProjectSaveErrorKind::SerializationFailed,
            .message = std::string("Failed to serialize project: ") + e.what(),
        });
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return Result<Unit, ProjectSaveError>::err(ProjectSaveError{
            .kind = ProjectSaveErrorKind::DirectoryNotWritable,
            .message = "Could not open " + path.string() + " for writing",
        });
    }

    file << j.dump(/*indent=*/2);
    return Result<Unit, ProjectSaveError>::ok(Unit{});
}

Project ProjectRepository::migrate(Project project) {
    // No prior schema versions exist yet; this is the seam future
    // migrations attach to (e.g. "if project.schemaVersion == '1.0' then
    // upgrade field X to shape Y and bump to '1.1'"). Always stamp the
    // current version on the way out so a manifest written by an older
    // binary that omitted some now-required field still ends up tagged
    // correctly after defaults are applied above by nlohmann's from_json.
    project.schemaVersion = ProjectRepository::kCurrentSchemaVersion;
    return project;
}

}  // namespace lazycmake::core
