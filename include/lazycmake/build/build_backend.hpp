#pragma once

// ==========================================================================
// IBuildBackend — strategy-pattern interface for build system backends (§11)
//
// LazyCMake supports multiple build systems (Ninja, Unix Makefiles, NMake)
// through this interface. Each concrete backend wraps the subprocess
// execution for its respective build tool.
//
// The interface has three operations:
//   - configure()  : run CMake configure (-S <src> -B <build> -G <generator>)
//   - build()      : run the build tool (ninja, make, nmake)
//   - clean()      : clean build artifacts
//
// All three operations are async: they return a BuildHandle that the caller
// can use to wait for completion or cancel the operation. Progress and
// completion are delivered via callbacks.
//
// The actual subprocess execution will use reproc (Phase 4+). For now,
// the interface is defined and test doubles can be used for unit testing
// the BuildManager state machine.
//
// Design (§11):
//   - Async, non-blocking: the build never blocks the UI thread.
//   - Streamed output: compiler output is captured line-by-line, not
//     buffered to completion, so the Output panel streams live.
//   - Cancellable: each operation returns a handle that can be cancelled.
// ==========================================================================

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace lazycmake::build {

// Represents the current status of an async build operation.
enum class BuildOperationStatus {
    Pending,        // Operation hasn't started yet.
    Running,        // Operation is in progress.
    Completed,      // Operation completed successfully.
    Failed,         // Operation completed with an error.
    Cancelled,      // Operation was cancelled by the user.
};

struct BuildResult {
    BuildOperationStatus status;
    int exitCode = -1;
    std::string output;     // Full captured output.
    std::string errorMsg;   // Human-readable error message.
};

// Handle for a running build operation. The caller can:
//   - Poll status with status().
//   - Wait for completion with wait().
//   - Cancel with cancel().
class BuildHandle {
public:
    virtual ~BuildHandle() = default;

    // Get the current status of the operation.
    [[nodiscard]] virtual BuildOperationStatus status() const = 0;

    // Block until the operation completes, or until the timeout expires.
    // Returns the final result of the operation.
    [[nodiscard]] virtual BuildResult wait(
        std::chrono::milliseconds timeout = std::chrono::milliseconds::max()) = 0;

    // Cancel a running operation. Returns immediately; the actual
    // termination may happen asynchronously. After calling cancel(),
    // status() will eventually return Cancelled.
    virtual void cancel() = 0;

    // Register a callback for each line of output.
    // The callback receives the stream name ("stdout" or "stderr")
    // and the line content. This is how the Output panel gets live
    // streaming output (§11).
    virtual void onOutput(std::function<void(const std::string& stream,
                                              const std::string& line)> callback) = 0;
};

// Build backend interface.
class IBuildBackend {
public:
    virtual ~IBuildBackend() = default;

    // Unique name of this backend (e.g. "Ninja", "Unix Makefiles").
    [[nodiscard]] virtual std::string name() const = 0;

    // Run CMake configure. The backend knows how to map its generator
    // name to the -G argument.
    //   sourceDir   : the project root (where CMakeLists.txt is).
    //   buildDir    : the build directory.
    //   options     : additional CMake options (e.g. -DCMAKE_BUILD_TYPE=Debug).
    [[nodiscard]] virtual std::unique_ptr<BuildHandle> configure(
        const std::string& sourceDir,
        const std::string& buildDir,
        const std::vector<std::string>& options = {}) = 0;

    // Run the build tool to build the entire project or a specific target.
    //   buildDir    : the build directory (already configured).
    //   target      : specific target to build (empty = default target).
    //   parallelJobs: number of parallel jobs (0 = let the tool decide).
    [[nodiscard]] virtual std::unique_ptr<BuildHandle> build(
        const std::string& buildDir,
        const std::string& target = "",
        int parallelJobs = 0) = 0;

    // Clean build artifacts.
    [[nodiscard]] virtual std::unique_ptr<BuildHandle> clean(
        const std::string& buildDir) = 0;

    // Get the CMake generator string for this backend.
    // e.g. "Ninja", "Unix Makefiles".
    [[nodiscard]] virtual std::string cmakeGeneratorName() const = 0;

    // Check if this backend is available on the current system.
    // Returns true if the build tool executable was found in PATH.
    [[nodiscard]] virtual bool isAvailable() const = 0;
};

// ==========================================================================
// NullBuildHandle — a test double / no-op implementation of BuildHandle
//
// This is useful for testing BuildManager without actually running a
// build. It immediately completes with a configurable status.
// ==========================================================================

class NullBuildHandle : public BuildHandle {
public:
    explicit NullBuildHandle(BuildOperationStatus immediateStatus = BuildOperationStatus::Completed,
                             int exitCode = 0);

    [[nodiscard]] BuildOperationStatus status() const override;
    [[nodiscard]] BuildResult wait(std::chrono::milliseconds timeout) override;
    void cancel() override;
    void onOutput(std::function<void(const std::string&, const std::string&)> callback) override;

private:
    BuildOperationStatus status_;
    int exitCode_;
    std::function<void(const std::string&, const std::string&)> outputCallback_;
};

// ==========================================================================
// FakeBuildBackend — a test double for IBuildBackend
//
// Always succeeds (or fails if configured to do so). Used by BuildManager
// tests to verify the state machine transitions without real subprocesses.
// ==========================================================================

class FakeBuildBackend : public IBuildBackend {
public:
    explicit FakeBuildBackend(bool shouldSucceed = true,
                               std::string name = "FakeBuild");

    std::string name() const override;

    std::unique_ptr<BuildHandle> configure(
        const std::string& sourceDir,
        const std::string& buildDir,
        const std::vector<std::string>& options) override;

    std::unique_ptr<BuildHandle> build(
        const std::string& buildDir,
        const std::string& target,
        int parallelJobs) override;

    std::unique_ptr<BuildHandle> clean(
        const std::string& buildDir) override;

    std::string cmakeGeneratorName() const override;
    bool isAvailable() const override;

private:
    bool shouldSucceed_;
    std::string name_;
};

} // namespace lazycmake::build
