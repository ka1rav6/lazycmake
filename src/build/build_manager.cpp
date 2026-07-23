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

    sourceDir_ = sourceDir;
    buildDir_ = buildDir;
    generator_ = generator;
    buildType_ = buildType;

    cancelRequested_ = false;
    workThread_ = std::thread([this, sourceDir, buildDir, generator, buildType]() {
        doConfigure(sourceDir, buildDir, generator, buildType);
    });
}

void BuildManager::build(const std::string& target) {
    if (isRunning()) {
        spdlog::warn("BuildManager: Cannot build while already running");
        return;
    }

    lastTarget_ = target;

    cancelRequested_ = false;
    workThread_ = std::thread([this, target]() {
        if (stage_ == BuildStage::Idle &&
            !sourceDir_.empty() && !buildDir_.empty()) {
            doConfigure(sourceDir_, buildDir_, generator_, buildType_);
            if (cancelRequested_ || stage_ == BuildStage::Failed) return;
        }
        doBuild(target);
    });
}

void BuildManager::cancel() {
    cancelRequested_ = true;
    if (currentHandle_) {
        currentHandle_->cancel();
    }
    if (workThread_.joinable()) {
        workThread_.join();
    }
    currentHandle_.reset();
    if (isRunning()) {
        transitionTo(BuildStage::Idle);
    }
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
    cancel();
    stage_ = BuildStage::Idle;
    lastResult_ = BuildResult{};
}

void BuildManager::setBackend(std::unique_ptr<IBuildBackend> backend) {
    cancel();
    backend_ = std::move(backend);
}

void BuildManager::setConfigureParams(const std::string& sourceDir,
                                       const std::string& buildDir,
                                       const std::string& generator,
                                       const std::string& buildType) {
    sourceDir_ = sourceDir;
    buildDir_ = buildDir;
    generator_ = generator;
    buildType_ = buildType;
}

void BuildManager::waitForCompletion() {
    if (workThread_.joinable()) {
        workThread_.join();
    }
}

void BuildManager::transitionTo(BuildStage newStage) {
    stage_ = newStage;
}

void BuildManager::doConfigure(const std::string& sourceDir,
                                const std::string& buildDir,
                                const std::string& generator,
                                const std::string& buildType) {
    if (cancelRequested_) return;

    transitionTo(BuildStage::Configuring);
    emitProgress("Configuring", 0, "Running cmake configure...");

    std::vector<std::string> options;
    if (!buildType.empty()) {
        options.push_back("-DCMAKE_BUILD_TYPE=" + buildType);
    }
    options.push_back("-G" + generator);

    auto handle = backend_->configure(sourceDir, buildDir, options);
    if (cancelRequested_) return;

    handle->onOutput([this](const std::string& stream, const std::string& line) {
        eventBus_.publishThreadSafe(events::BuildProgressEvent{
            .stage = stream == "stderr" ? "Configuring" : "Configuring",
            .percentComplete = -1,
            .detail = line,
        });
    });

    currentHandle_ = std::move(handle);
    onConfigureComplete(std::move(currentHandle_));
}

void BuildManager::doBuild(const std::string& target) {
    if (cancelRequested_) return;

    transitionTo(BuildStage::Building);
    emitProgress("Building", 0,
                  target.empty() ? "Building all targets..." : "Building " + target + "...");

    auto handle = backend_->build(buildDir_, target);
    if (cancelRequested_) return;

    handle->onOutput([this](const std::string& stream, const std::string& line) {
        eventBus_.publishThreadSafe(events::BuildProgressEvent{
            .stage = stream == "stderr" ? "Building" : "Building",
            .percentComplete = -1,
            .detail = line,
        });
    });

    currentHandle_ = std::move(handle);
    onBuildComplete(std::move(currentHandle_));
}

void BuildManager::onConfigureComplete(std::unique_ptr<BuildHandle> handle) {
    if (cancelRequested_) return;

    BuildResult result = handle->wait();
    lastResult_ = result;

    if (result.status == BuildOperationStatus::Completed) {
        transitionTo(BuildStage::Generating);
        emitProgress("Generating", 50, "Configuration succeeded, generating...");

        transitionTo(BuildStage::Succeeded);
        emitProgress("Succeeded", 100, "Configuration complete");

        eventBus_.publishThreadSafe(events::ConfigureFinishedEvent{
            .success = true,
            .output = result.output,
            .errorMsg = {},
        });
    } else {
        transitionTo(BuildStage::Failed);
        emitProgress("Failed", 100, "Configuration failed: " + result.errorMsg);

        eventBus_.publishThreadSafe(events::ConfigureFinishedEvent{
            .success = false,
            .output = result.output,
            .errorMsg = result.errorMsg,
        });
    }
}

void BuildManager::onBuildComplete(std::unique_ptr<BuildHandle> handle) {
    if (cancelRequested_) return;

    BuildResult result = handle->wait();
    lastResult_ = result;

    if (result.status == BuildOperationStatus::Completed) {
        transitionTo(BuildStage::Succeeded);
        emitProgress("Succeeded", 100, "Build complete");

        eventBus_.publishThreadSafe(events::BuildFinishedEvent{
            .success = true,
            .targetName = lastTarget_,
            .exitCode = result.exitCode,
            .output = result.output,
        });
    } else if (result.status == BuildOperationStatus::Cancelled) {
        transitionTo(BuildStage::Idle);
        emitProgress("Cancelled", 0, "Build cancelled");

        eventBus_.publishThreadSafe(events::BuildFinishedEvent{
            .success = false,
            .targetName = lastTarget_,
            .exitCode = -1,
            .output = "Build was cancelled",
        });
    } else {
        transitionTo(BuildStage::Failed);
        emitProgress("Failed", 100, "Build failed: " + result.errorMsg);

        eventBus_.publishThreadSafe(events::BuildFinishedEvent{
            .success = false,
            .targetName = lastTarget_,
            .exitCode = result.exitCode,
            .output = result.output,
        });
    }
}

void BuildManager::emitProgress(const std::string& stage, int percent,
                                 const std::string& detail) {
    eventBus_.publishThreadSafe(events::BuildProgressEvent{
        .stage = stage,
        .percentComplete = percent,
        .detail = detail,
    });
}

} // namespace lazycmake::build
