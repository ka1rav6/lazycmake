#include "lazycmake/core/target_validator.hpp"

#include <set>
#include <unordered_map>
#include <unordered_set>

namespace lazycmake::core {

std::vector<ValidationIssue> TargetValidator::validate(const Project& project) {
    std::vector<ValidationIssue> issues;
    checkDuplicateNames(project, issues);
    checkSelfLinks(project, issues);
    checkUnknownLinkTargets(project, issues);
    checkCyclicLinks(project, issues);
    checkEmptySources(project, issues);
    return issues;
}

void TargetValidator::checkDuplicateNames(const Project& project,
                                            std::vector<ValidationIssue>& issues) {
    std::unordered_map<std::string, int> counts;
    for (const auto& target : project.targets) {
        counts[target.name]++;
    }
    for (const auto& [name, count] : counts) {
        if (count > 1) {
            issues.push_back(ValidationIssue{
                .severity = ValidationSeverity::Error,
                .message = "Duplicate target name: '" + name + "' is used by " +
                            std::to_string(count) + " targets",
                .targetName = name,
            });
        }
    }
}

void TargetValidator::checkSelfLinks(const Project& project,
                                       std::vector<ValidationIssue>& issues) {
    for (const auto& target : project.targets) {
        for (const auto& link : target.linkTargets) {
            if (link == target.name) {
                issues.push_back(ValidationIssue{
                    .severity = ValidationSeverity::Error,
                    .message = "Target '" + target.name + "' links against itself",
                    .targetName = target.name,
                });
            }
        }
    }
}

void TargetValidator::checkUnknownLinkTargets(const Project& project,
                                                std::vector<ValidationIssue>& issues) {
    // A linkTargets entry is valid if it names either another Target in
    // this Project, or an enabled DependencySpec's link name (e.g.
    // "fmt::fmt"). Anything else is likely a typo and should be flagged
    // rather than silently passed through to the generated CMake, where
    // it would only surface as a confusing CMake-level configure error.
    std::unordered_set<std::string> knownNames;
    for (const auto& target : project.targets) {
        knownNames.insert(target.name);
    }
    for (const auto& dep : project.dependencies) {
        if (!dep.enabled) continue;
        for (const auto& linkName : dep.linkTargets) {
            knownNames.insert(linkName);
        }
    }

    for (const auto& target : project.targets) {
        for (const auto& link : target.linkTargets) {
            if (link != target.name && !knownNames.contains(link)) {
                issues.push_back(ValidationIssue{
                    .severity = ValidationSeverity::Warning,
                    .message = "Target '" + target.name + "' links against unknown name '" +
                                link + "' (not a project target or an enabled dependency)",
                    .targetName = target.name,
                });
            }
        }
    }
}

void TargetValidator::checkCyclicLinks(const Project& project,
                                         std::vector<ValidationIssue>& issues) {
    // Standard DFS cycle detection restricted to edges between known
    // in-project targets (dependency link names are leaves, not part of
    // the graph we can cycle through).
    std::unordered_set<std::string> projectTargetNames;
    for (const auto& target : project.targets) {
        projectTargetNames.insert(target.name);
    }

    enum class VisitState { Unvisited, InProgress, Done };
    std::unordered_map<std::string, VisitState> state;
    for (const auto& target : project.targets) {
        state[target.name] = VisitState::Unvisited;
    }

    std::set<std::string> alreadyReported;

    auto dfs = [&](const std::string& startName, auto&& dfsRef) -> bool {
        state[startName] = VisitState::InProgress;
        const Target* target = project.findTarget(startName);
        if (target != nullptr) {
            for (const auto& link : target->linkTargets) {
                if (!projectTargetNames.contains(link)) continue;
                if (state[link] == VisitState::InProgress) {
                    if (alreadyReported.insert(link).second) {
                        issues.push_back(ValidationIssue{
                            .severity = ValidationSeverity::Error,
                            .message = "Cyclic link dependency detected involving target '" +
                                        link + "'",
                            .targetName = link,
                        });
                    }
                    return true;
                }
                if (state[link] == VisitState::Unvisited) {
                    if (dfsRef(link, dfsRef)) return true;
                }
            }
        }
        state[startName] = VisitState::Done;
        return false;
    };

    for (const auto& target : project.targets) {
        if (state[target.name] == VisitState::Unvisited) {
            dfs(target.name, dfs);
        }
    }
}

void TargetValidator::checkEmptySources(const Project& project,
                                          std::vector<ValidationIssue>& issues) {
    for (const auto& target : project.targets) {
        // Interface libraries are legitimately header-only / source-free.
        if (target.kind == TargetKind::InterfaceLibrary) continue;
        if (target.sources.empty()) {
            issues.push_back(ValidationIssue{
                .severity = ValidationSeverity::Warning,
                .message = "Target '" + target.name + "' has no source files",
                .targetName = target.name,
            });
        }
    }
}

}  // namespace lazycmake::core
