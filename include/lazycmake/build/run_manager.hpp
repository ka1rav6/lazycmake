#pragma once

// ==========================================================================
// RunManager — launches built executables with streaming output (§9.2)
//
// State machine:
//   Idle ─ run() ──► Launching ──► Running ──┬─ exit(code) ──► Finished
//                                             └─ kill()      ──► Killed
//
// The RunManager streams stdout/stderr line-by-line via EventBus
// (RunOutputEvent), so the Run overlay can display output in real time.
//
// Like BuildManager, the RunManager uses an async subprocess execution
// model. For unit testing, a FakeRunner can be injected.
// ==========================================================================

#include <memory>
#include <string>
#include <vector>

#include "lazycmake/events/event_bus.hpp"

namespace lazycmake::build {

// The current state of a run.
enum class RunState {
    Idle,
    Launching,
    Running,
    Finished,
    Killed,
};

[[nodiscard]] std::string toString(RunState state);

// Result of a completed run.
struct RunResult {
    bool success = false;
    int exitCode = -1;
    std::string output;         // Full captured output.
    std::string errorMsg;       // Error message if failed to launch.
};

// Interface for a run backend (can be faked in tests).
class IRunBackend {
public:
    virtual ~IRunBackend() = default;

    // Launch an executable with the given arguments, working directory,
    // and environment variables. Returns true if the process was
    // successfully launched.
    //   executablePath: path to the executable to run.
    //   args          : command-line arguments.
    //   workingDir    : working directory for the process.
    //   envVars       : environment variables (KEY=VALUE format).
    //   onOutput      : callback for each line of stdout/stderr.
    virtual bool launch(
        const std::string& executablePath,
        const std::vector<std::string>& args,
        const std::string& workingDir,
        const std::vector<std::string>& envVars,
        std::function<void(const std::string& stream,
                            const std::string& line)> onOutput) = 0;

    // Wait for the process to finish, with an optional timeout.
    virtual RunResult wait(int timeoutMs = 0) = 0;

    // Kill the running process.
    virtual void kill() = 0;

    // Check if the process is still running.
    [[nodiscard]] virtual bool isRunning() const = 0;

    // Get the PID of the running process (or -1 if not running).
    [[nodiscard]] virtual int pid() const = 0;
};

// Fake implementation of IRunBackend for testing.
class FakeRunBackend : public IRunBackend {
public:
    explicit FakeRunBackend(bool shouldSucceed = true, int exitCode = 0);

    bool launch(const std::string& executablePath,
                const std::vector<std::string>& args,
                const std::string& workingDir,
                const std::vector<std::string>& envVars,
                std::function<void(const std::string&, const std::string&)> onOutput) override;

    RunResult wait(int timeoutMs) override;
    void kill() override;
    bool isRunning() const override;
    int pid() const override;

    // Access captured launch parameters for test assertions.
    [[nodiscard]] const std::string& lastExecutable() const { return lastExecutable_; }
    [[nodiscard]] const std::vector<std::string>& lastArgs() const { return lastArgs_; }

private:
    bool shouldSucceed_;
    int exitCode_;
    bool launched_ = false;
    bool killed_ = false;
    std::string lastExecutable_;
    std::vector<std::string> lastArgs_;
    std::function<void(const std::string&, const std::string&)> outputCallback_;
};

class RunManager {
public:
    explicit RunManager(events::EventBus& eventBus);

    // Set the run backend (use FakeRunBackend for testing).
    void setBackend(std::unique_ptr<IRunBackend> backend);

    // Run an executable.
    //   executablePath : path to the executable.
    //   args           : command-line arguments.
    //   workingDir     : working directory (empty = project root).
    //   envVars        : environment variables (KEY=VALUE format).
    //
    // Returns immediately. Output is streamed via EventBus (RunOutputEvent).
    // When the process finishes, a RunFinishedEvent is published.
    void run(const std::string& executablePath,
             const std::vector<std::string>& args = {},
             const std::string& workingDir = "",
             const std::vector<std::string>& envVars = {});

    // Kill the currently running process.
    void kill();

    // Get the current run state.
    [[nodiscard]] RunState currentState() const;

    // Check if a process is currently running.
    [[nodiscard]] bool isRunning() const;

    // Get the last run result.
    [[nodiscard]] const RunResult& lastResult() const;

private:
    events::EventBus& eventBus_;
    std::unique_ptr<IRunBackend> backend_;
    RunState state_ = RunState::Idle;
    RunResult lastResult_;

    void transitionTo(RunState newState);
};

} // namespace lazycmake::build
