#pragma once

#include <string>
#include <vector>

#include "lazycmake/core/project.hpp"

namespace lazycmake::core {

enum class ValidationSeverity {
    Error,
    Warning
};

struct ValidationIssue {
    ValidationSeverity severity;
    std::string message;
    std::string targetName;  // empty if the issue isn't target-specific

    bool operator==(const ValidationIssue& other) const = default;
};

// Pure, side-effect-free validation of a Project's target graph. Used by
// both the TUI (to block "Generate" with a clear message) and by tests
// (Catch2 cases run this directly against in-memory Project fixtures with
// no filesystem or CMake involved at all — see docs/DESIGN.md §18).
class TargetValidator {
public:
    [[nodiscard]] static std::vector<ValidationIssue> validate(const Project& project);

private:
    static void checkDuplicateNames(const Project& project, std::vector<ValidationIssue>& issues);
    static void checkSelfLinks(const Project& project, std::vector<ValidationIssue>& issues);
    static void checkUnknownLinkTargets(const Project& project, std::vector<ValidationIssue>& issues);
    static void checkCyclicLinks(const Project& project, std::vector<ValidationIssue>& issues);
    static void checkEmptySources(const Project& project, std::vector<ValidationIssue>& issues);
};

}  // namespace lazycmake::core
