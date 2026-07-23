#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "lazycmake/build/build_backend.hpp"
#include "lazycmake/events/event_bus.hpp"

namespace lazycmake::build {

enum class BuildStage {
    Idle,
    Configuring,
    Generating,
    Building,
    Linking,
    Succeeded,
    Failed,
};

[[nodiscard]] std::string toString(BuildStage stage);

class BuildManager {
public:
    explicit BuildManager(std::unique_ptr<IBuildBackend> backend,
                           events::EventBus& eventBus);

    ~BuildManager();

    void configure(const std::string& sourceDir,
                    const std::string& buildDir,
                    const std::string& generator,
                    const std::string& buildType);

    void build(const std::string& target = "");

    void cancel();

    [[nodiscard]] BuildStage currentStage() const;

    [[nodiscard]] bool isRunning() const;

    [[nodiscard]] const BuildResult& lastResult() const;

    void reset();

    void setBackend(std::unique_ptr<IBuildBackend> backend);

    // Set configure parameters for auto-configure on next build().
    // If build() is called while Idle and these are non-empty, configure()
    // runs automatically before the build in the background thread.
    void setConfigureParams(const std::string& sourceDir,
                            const std::string& buildDir,
                            const std::string& generator,
                            const std::string& buildType);

    // Block until the current background operation finishes.
    // Used by tests that need synchronous completion guarantees.
    void waitForCompletion();

private:
    std::unique_ptr<IBuildBackend> backend_;
    events::EventBus& eventBus_;
    BuildStage stage_ = BuildStage::Idle;
    BuildResult lastResult_;

    std::string sourceDir_;
    std::string buildDir_;
    std::string generator_;
    std::string buildType_;
    std::string lastTarget_;

    std::unique_ptr<BuildHandle> currentHandle_;

    // Background thread for async operations.
    std::thread workThread_;
    std::atomic<bool> cancelRequested_{false};

    void transitionTo(BuildStage newStage);

    // Called on the background thread to perform configure + wait.
    void doConfigure(const std::string& sourceDir,
                     const std::string& buildDir,
                     const std::string& generator,
                     const std::string& buildType);

    // Called on the background thread to perform build + wait.
    void doBuild(const std::string& target);

    void onConfigureComplete(std::unique_ptr<BuildHandle> handle);
    void onBuildComplete(std::unique_ptr<BuildHandle> handle);

    void emitProgress(const std::string& stage, int percent = -1,
                       const std::string& detail = "");
};

} // namespace lazycmake::build
