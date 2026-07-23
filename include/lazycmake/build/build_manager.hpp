#pragma once

// ==========================================================================
// BuildManager — orchestrates the build pipeline state machine (§9.1)
//
// State machine:
//   Idle ─ configure() ──► Configuring ──┬─ success ──► Generating ──┬─ success ──► Building ──┬─ success ──► Succeeded
//     │                                    │ (cancel/error)           │ (cancel/error)          │ (cancel/error)
//     │                                    ▼                          ▼                         ▼
//     └──────────────────────────── Idle ◄─────────────────────── Idle ◄──────────────────── Idle
//
// Each transition emits a BuildProgressEvent (via EventBus) consumed by
// the Build overlay's checklist UI and by any plugin implementing IBuildStep.
//
// The BuildManager uses IBuildBackend for the actual subprocess execution.
// In tests, a FakeBuildBackend is injected to verify state machine
// transitions without running real processes (§18).
//
// Key properties (§11):
//   - Non-blocking API: configure()/build() return immediately; progress
//     is delivered via EventBus events.
//   - Cancellable: cancel() transitions back to Idle.
//   - Stage tracking: the current stage is always queryable.
// ==========================================================================

#include <memory>
#include <string>

#include "lazycmake/build/build_backend.hpp"
#include "lazycmake/events/event_bus.hpp"

namespace lazycmake::build {

// The current stage of the build pipeline.
enum class BuildStage {
    Idle,
    Configuring,
    Generating,
    Building,
    Linking,
    Succeeded,
    Failed,
};

// Convert BuildStage to a human-readable string.
[[nodiscard]] std::string toString(BuildStage stage);

class BuildManager {
public:
    // Create a BuildManager that uses the given backend and event bus.
    // The backend can be a NinjaBackend, MakeBackend, or FakeBuildBackend
    // (for testing). The event bus is where progress events are published.
    explicit BuildManager(std::unique_ptr<IBuildBackend> backend,
                           events::EventBus& eventBus);

    ~BuildManager();

    // Run CMake configure for the given project.
    //   sourceDir : project root (where CMakeLists.txt lives).
    //   buildDir  : build output directory.
    //   generator : CMake generator string (e.g. "Ninja", "Unix Makefiles").
    //   buildType : Debug/Release/etc.
    //
    // Returns immediately; progress is delivered via EventBus.
    // Has no effect if the pipeline is already running.
    void configure(const std::string& sourceDir,
                    const std::string& buildDir,
                    const std::string& generator,
                    const std::string& buildType);

    // Build the entire project or a specific target.
    // The pipeline: Configure (if needed) → Generate → Build → Link.
    // If the project is already configured, configure is skipped.
    // Returns immediately; progress is delivered via EventBus.
    void build(const std::string& target = "");

    // Cancel the current operation. Transitions to Idle.
    // The subprocess is killed if one is running.
    void cancel();

    // Get the current stage of the pipeline.
    [[nodiscard]] BuildStage currentStage() const;

    // Check if the pipeline is currently running (any stage other than
    // Idle, Succeeded, or Failed).
    [[nodiscard]] bool isRunning() const;

    // Get the last build result.
    [[nodiscard]] const BuildResult& lastResult() const;

    // Reset the pipeline to Idle (e.g., after a failed build).
    void reset();

    // Set the backend (useful for injecting test doubles after construction).
    void setBackend(std::unique_ptr<IBuildBackend> backend);

private:
    std::unique_ptr<IBuildBackend> backend_;
    events::EventBus& eventBus_;
    BuildStage stage_ = BuildStage::Idle;
    BuildResult lastResult_;

    // Stored configure parameters for automatic re-configure.
    std::string sourceDir_;
    std::string buildDir_;
    std::string generator_;
    std::string buildType_;

    // The handle for the currently running operation.
    std::unique_ptr<BuildHandle> currentHandle_;

    // Internal state machine transitions.
    void transitionTo(BuildStage newStage);
    void onConfigureComplete(std::unique_ptr<BuildHandle> handle);
    void onBuildComplete(std::unique_ptr<BuildHandle> handle);

    // Emit a progress event on the bus.
    void emitProgress(const std::string& stage, int percent = -1,
                       const std::string& detail = "");
};

} // namespace lazycmake::build
