#include "lazycmake/build/build_backend.hpp"

#include <algorithm>
#include <iostream>

namespace lazycmake::build {

// ==========================================================================
// NullBuildHandle implementation
// ==========================================================================

NullBuildHandle::NullBuildHandle(BuildOperationStatus immediateStatus, int exitCode)
    : status_(immediateStatus)
    , exitCode_(exitCode) {}

BuildOperationStatus NullBuildHandle::status() const {
    return status_;
}

BuildResult NullBuildHandle::wait(std::chrono::milliseconds /*timeout*/) {
    return BuildResult{
        .status = status_,
        .exitCode = exitCode_,
        .output = {},
        .errorMsg = (status_ == BuildOperationStatus::Failed) ? "Operation failed (fake)" : "",
    };
}

void NullBuildHandle::cancel() {
    if (status_ == BuildOperationStatus::Pending ||
        status_ == BuildOperationStatus::Running) {
        status_ = BuildOperationStatus::Cancelled;
    }
}

void NullBuildHandle::onOutput(
    std::function<void(const std::string&, const std::string&)> callback) {
    outputCallback_ = std::move(callback);
}

// ==========================================================================
// FakeBuildBackend implementation
// ==========================================================================

FakeBuildBackend::FakeBuildBackend(bool shouldSucceed, std::string name)
    : shouldSucceed_(shouldSucceed)
    , name_(std::move(name)) {}

std::string FakeBuildBackend::name() const {
    return name_;
}

std::unique_ptr<BuildHandle> FakeBuildBackend::configure(
    const std::string& /*sourceDir*/,
    const std::string& /*buildDir*/,
    const std::vector<std::string>& /*options*/) {
    auto status = shouldSucceed_ ? BuildOperationStatus::Completed
                                 : BuildOperationStatus::Failed;
    return std::make_unique<NullBuildHandle>(status, shouldSucceed_ ? 0 : 1);
}

std::unique_ptr<BuildHandle> FakeBuildBackend::build(
    const std::string& /*buildDir*/,
    const std::string& /*target*/,
    int /*parallelJobs*/) {
    auto status = shouldSucceed_ ? BuildOperationStatus::Completed
                                 : BuildOperationStatus::Failed;
    return std::make_unique<NullBuildHandle>(status, shouldSucceed_ ? 0 : 1);
}

std::unique_ptr<BuildHandle> FakeBuildBackend::clean(
    const std::string& /*buildDir*/) {
    auto status = shouldSucceed_ ? BuildOperationStatus::Completed
                                 : BuildOperationStatus::Failed;
    return std::make_unique<NullBuildHandle>(status, shouldSucceed_ ? 0 : 1);
}

std::string FakeBuildBackend::cmakeGeneratorName() const {
    return "Fake";
}

bool FakeBuildBackend::isAvailable() const {
    return true;
}

} // namespace lazycmake::build
