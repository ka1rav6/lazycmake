#include "lazycmake/build/build_manager.hpp"

#include <spdlog/spdlog.h>
#include <utility>

namespace lazycmake::build {

std::string toString(BuildStage stage) {
    switch (stage) {
        case BuildStage::Idle: return "Idle";
        case BuildStage::Configuring: return "Configuring";
        case BuildStage::Generating: return "Generating";
        case BuildStage::Building: return "Building";
        case BuildStage::Linking: return "Linking";
        case BuildStage::Succeeded: return "Succeeded";
        case BuildStage::Failed: return "Failed";
    }
    return "Unknown";
}

BuildManager::BuildManager(std::unique_ptr<IBuildBackend> backend,
                             events::EventBus& eventBus)
    : backend_(std::move(backend))
    , eventBus_(eventBus) {}

BuildManager::~BuildManager() {
    cancel();
}

void BuildManager::configure(const std::string& sourceDir,
                              const std::string& buildDir,
                              const std::string& generator,
                              const std::string& buildType) {
    if (isRunning()) {
        spdlog::warn("BuildManager: Cannot configure while a build is running");
        return;
    }

    // Store parameters for potential automatic re-configure.
    sourceDir_ = sourceDir;
    buildDir_ = buildDir;
    generator_ = generator;
    buildType_ = buildType;

    transitionTo(BuildStage::Configuring);
    emitProgress("Configuring", 0, "Running cmake configure...");

    // Start the configure operation.
    std::vector<std::string> options;
    if (!buildType_.empty()) {
        options.push_back("-DCMAKE_BUILD_TYPE=" + buildType_);
    }
    options.push_back("-G" + generator_);

    currentHandle_ = backend_->configure(sourceDir_, buildDir_, options);

    // In a real implementation, this would be async. For now, we
    // synchronously wait and process the result. A future Phase 6
    // enhancement would make this truly async using reproc + threads.
    onConfigureComplete(std::move(currentHandle_));
}

void BuildManager::build(const std::string& target) {
    if (isRunning()) {
        spdlog::warn("BuildManager: Cannot build while already running");
        return;
    }

    // If we have configure parameters and the state is Idle, run configure
    // first (if autoConfigure is enabled — this is checked by the caller).
    // For now, we go directly to building and assume the project is
    // already configured.

    transitionTo(BuildStage::Building);
    emitProgress("Building", 0,
                  target.empty() ? "Building all targets..." : "Building " + target + "...");

    currentHandle_ = backend_->build(buildDir_, target);

    // Synchronous wait for now (async in future).
    onBuildComplete(std::move(currentHandle_));
}

void BuildManager::cancel() {
    if (isRunning()) {
        if (currentHandle_) {
            currentHandle_->cancel();
            // Wait briefly for cancellation.
            currentHandle_->wait(std::chrono::milliseconds(100));
            currentHandle_.reset();
        }
        transitionTo(BuildStage::Idle);
    }
    // If we're in a terminal state (Succeeded, Failed, Idle), cancel is a no-op.
}

BuildStage BuildManager::currentStage() const {
    return stage_;
}

bool BuildManager::isRunning() const {
    return stage_ == BuildStage::Configuring
        || stage_ == BuildStage::Generating
        || stage_ == BuildStage::Building
        || stage_ == BuildStage::Linking;
}

const BuildResult& BuildManager::lastResult() const {
    return lastResult_;
}

void BuildManager::reset() {
    if (isRunning()) {
        cancel();
    }
    stage_ = BuildStage::Idle;
    lastResult_ = BuildResult{};
}

void BuildManager::setBackend(std::unique_ptr<IBuildBackend> backend) {
    backend_ = std::move(backend);
}

void BuildManager::transitionTo(BuildStage newStage) {
    stage_ = newStage;
}

void BuildManager::onConfigureComplete(std::unique_ptr<BuildHandle> handle) {
    BuildResult result = handle->wait();
    lastResult_ = result;

    if (result.status == BuildOperationStatus::Completed) {
        transitionTo(BuildStage::Generating);
        emitProgress("Generating", 50, "Configuration succeeded, generating...");

        // In a full implementation, we'd run the CMake generator here
        // (ModernCMakeGenerator). For now, we just emit a progress event
        // and move to the next stage.

        transitionTo(BuildStage::Succeeded);
        emitProgress("Succeeded", 100, "Configuration complete");

        // Publish the success event.
        eventBus_.publish(events::ConfigureFinishedEvent{
            .success = true,
            .output = result.output,
            .errorMsg = {},
        });
    } else {
        transitionTo(BuildStage::Failed);
        emitProgress("Failed", 100, "Configuration failed: " + result.errorMsg);

        eventBus_.publish(events::ConfigureFinishedEvent{
            .success = false,
            .output = result.output,
            .errorMsg = result.errorMsg,
        });
    }
}

void BuildManager::onBuildComplete(std::unique_ptr<BuildHandle> handle) {
    BuildResult result = handle->wait();
    lastResult_ = result;

    if (result.status == BuildOperationStatus::Completed) {
        transitionTo(BuildStage::Succeeded);
        emitProgress("Succeeded", 100, "Build complete");

        eventBus_.publish(events::BuildFinishedEvent{
            .success = true,
            .targetName = {},
            .exitCode = result.exitCode,
            .output = result.output,
        });
    } else if (result.status == BuildOperationStatus::Cancelled) {
        transitionTo(BuildStage::Idle);
        emitProgress("Cancelled", 0, "Build cancelled");

        eventBus_.publish(events::BuildFinishedEvent{
            .success = false,
            .targetName = {},
            .exitCode = -1,
            .output = "Build was cancelled",
        });
    } else {
        transitionTo(BuildStage::Failed);
        emitProgress("Failed", 100, "Build failed: " + result.errorMsg);

        eventBus_.publish(events::BuildFinishedEvent{
            .success = false,
            .targetName = {},
            .exitCode = result.exitCode,
            .output = result.output,
        });
    }
}

void BuildManager::emitProgress(const std::string& stage, int percent,
                                 const std::string& detail) {
    eventBus_.publish(events::BuildProgressEvent{
        .stage = stage,
        .percentComplete = percent,
        .detail = detail,
    });
}

} // namespace lazycmake::build
