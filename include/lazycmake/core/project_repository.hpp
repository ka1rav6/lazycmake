#pragma once

#include <filesystem>
#include <string>
#include <variant>

#include "lazycmake/core/project.hpp"

namespace lazycmake::core {

enum class ProjectLoadErrorKind {
    FileNotFound,
    InvalidJson,
    UnsupportedSchemaVersion,
    MissingRequiredField,
};

struct ProjectLoadError {
    ProjectLoadErrorKind kind;
    std::string message;
};

enum class ProjectSaveErrorKind {
    DirectoryNotWritable,
    SerializationFailed,
};

struct ProjectSaveError {
    ProjectSaveErrorKind kind;
    std::string message;
};

// Minimal C++20-compatible result type (the codebase targets C++20 as a
// floor per docs/DESIGN.md §3, so std::expected — C++23 — is avoided here).
// Exposes just enough of std::expected's surface for this repository's use.
template <typename T, typename E>
class Result {
public:
    static Result ok(T value) { return Result(std::move(value)); }
    static Result err(E error) { return Result(std::move(error), Tag{}); }

    [[nodiscard]] bool has_value() const { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const { return has_value(); }

    [[nodiscard]] const T& value() const { return std::get<T>(storage_); }
    [[nodiscard]] T& value() { return std::get<T>(storage_); }
    [[nodiscard]] const E& error() const { return std::get<E>(storage_); }

private:
    struct Tag {};
    explicit Result(T value) : storage_(std::move(value)) {}
    Result(E error, Tag) : storage_(std::move(error)) {}

    std::variant<T, E> storage_;
};

// Marker type standing in for "no value" in the success case of save(),
// since Result<T, E> needs a concrete T (unlike std::expected<void, E>).
struct Unit {};

// The only Core class that touches the project manifest file
// (`.lazycmake/project.json`) on disk. Kept deliberately narrow so that
// every other Core class operates purely on the in-memory Project struct,
// which is what makes them unit-testable without a filesystem.
//
// Schema migration: this repository is also where a future schema version
// bump gets handled — `migrate()` is a no-op today (schemaVersion "1.0" is
// the only version that exists) but the seam is here so that adding "1.1"
// later doesn't require touching callers.
class ProjectRepository {
public:
    static constexpr const char* kManifestRelativePath = ".lazycmake/project.json";
    static constexpr const char* kCurrentSchemaVersion = "1.0";

    [[nodiscard]] Result<Project, ProjectLoadError> load(
        const std::filesystem::path& projectRoot) const;

    [[nodiscard]] Result<Unit, ProjectSaveError> save(const Project& project) const;

private:
    [[nodiscard]] static std::filesystem::path manifestPath(
        const std::filesystem::path& projectRoot);

    // Returns the project unchanged if already at kCurrentSchemaVersion.
    [[nodiscard]] static Project migrate(Project project);
};

}  // namespace lazycmake::core
