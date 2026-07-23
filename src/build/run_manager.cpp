#include "lazycmake/build/run_manager.hpp"

#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>
#include <utility>

namespace lazycmake::build {

std::string toString(RunState state) {
    switch (state) {
        case RunState::Idle: return "Idle";
        case RunState::Launching: return "Launching";
        case RunState::Running: return "Running";
        case RunState::Finished: return "Finished";
        case RunState::Killed: return "Killed";
    }
    return "Unknown";
}

FakeRunBackend::FakeRunBackend(bool shouldSucceed, int exitCode)
    : shouldSucceed_(shouldSucceed)
    , exitCode_(exitCode) {}

bool FakeRunBackend::launch(const std::string& executablePath,
                             const std::vector<std::string>& args,
                             const std::string& /*workingDir*/,
                             const std::vector<std::string>& /*envVars*/,
                             std::function<void(const std::string&, const std::string&)> onOutput) {
    lastExecutable_ = executablePath;
    lastArgs_ = args;
    outputCallback_ = std::move(onOutput);
    launched_ = true;
    killed_ = false;

    if (outputCallback_) {
        outputCallback_("stdout", "Launching " + executablePath + "...");
        outputCallback_("stdout", "Running with " + std::to_string(args.size()) + " argument(s)");
    }

    return true;
}

RunResult FakeRunBackend::wait(int /*timeoutMs*/) {
    if (!launched_) {
        return RunResult{
            .success = false,
            .exitCode = -1,
            .output = {},
            .errorMsg = "Process was never launched",
        };
    }

    if (killed_) {
        return RunResult{
            .success = false,
            .exitCode = -1,
            .output = "Process was killed",
            .errorMsg = "Killed",
        };
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    if (outputCallback_) {
        if (shouldSucceed_) {
            outputCallback_("stdout", "Process completed successfully");
        } else {
            outputCallback_("stderr", "Process failed with exit code " + std::to_string(exitCode_));
        }
    }

    return RunResult{
        .success = shouldSucceed_,
        .exitCode = exitCode_,
        .output = {},
        .errorMsg = shouldSucceed_ ? "" : "Simulated failure",
    };
}

void FakeRunBackend::kill() {
    killed_ = true;
    if (outputCallback_) {
        outputCallback_("stdout", "Process killed by user");
    }
}

bool FakeRunBackend::isRunning() const {
    return launched_ && !killed_;
}

int FakeRunBackend::pid() const {
    return launched_ ? 12345 : -1;
}

RunManager::RunManager(events::EventBus& eventBus)
    : eventBus_(eventBus)
    , backend_(std::make_unique<FakeRunBackend>()) {}

RunManager::~RunManager() {
    kill();
}

void RunManager::setBackend(std::unique_ptr<IRunBackend> backend) {
    kill();
    backend_ = std::move(backend);
}

void RunManager::run(const std::string& executablePath,
                      const std::vector<std::string>& args,
                      const std::string& workingDir,
                      const std::vector<std::string>& envVars) {
    if (isRunning()) {
        spdlog::warn("RunManager: A process is already running");
        return;
    }

    cancelRequested_ = false;
    workThread_ = std::thread([this, executablePath, args, workingDir, envVars]() {
        doRun(executablePath, args, workingDir, envVars);
    });
}

void RunManager::doRun(const std::string& executablePath,
                        const std::vector<std::string>& args,
                        const std::string& workingDir,
                        const std::vector<std::string>& envVars) {
    if (cancelRequested_) return;

    transitionTo(RunState::Launching);

    eventBus_.publishThreadSafe(events::RunStartedEvent{
        .targetName = executablePath,
        .executablePath = executablePath,
    });

    bool launched = backend_->launch(
        executablePath, args, workingDir, envVars,
        [this](const std::string& stream, const std::string& line) {
            eventBus_.publishThreadSafe(events::RunOutputEvent{
                .stream = stream,
                .line = line,
            });
        });

    if (!launched) {
        transitionTo(RunState::Finished);

        RunResult result{
            .success = false,
            .exitCode = -1,
            .output = {},
            .errorMsg = "Failed to launch " + executablePath,
        };
        lastResult_ = result;

        eventBus_.publishThreadSafe(events::RunFinishedEvent{
            .success = false,
            .exitCode = -1,
        });
        return;
    }

    if (cancelRequested_) return;

    transitionTo(RunState::Running);

    RunResult result = backend_->wait(/*timeoutMs=*/0);
    lastResult_ = result;

    // Fix #5: always transition to Finished after wait(). Killed is only
    // set by the explicit kill() path, never from a failed exit code.
    transitionTo(RunState::Finished);

    eventBus_.publishThreadSafe(events::RunFinishedEvent{
        .success = result.success,
        .exitCode = result.exitCode,
    });
}

void RunManager::kill() {
    cancelRequested_ = true;
    if (backend_) {
        backend_->kill();
    }
    if (workThread_.joinable()) {
        workThread_.join();
    }
    transitionTo(RunState::Killed);
}

RunState RunManager::currentState() const {
    return state_;
}

bool RunManager::isRunning() const {
    return state_ == RunState::Launching || state_ == RunState::Running;
}

const RunResult& RunManager::lastResult() const {
    return lastResult_;
}

void RunManager::waitForCompletion() {
    if (workThread_.joinable()) {
        workThread_.join();
    }
}

void RunManager::transitionTo(RunState newState) {
    state_ = newState;
}

} // namespace lazycmake::build
