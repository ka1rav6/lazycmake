#pragma once

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "lazycmake/events/event_bus.hpp"

namespace lazycmake::build {

enum class RunState {
    Idle,
    Launching,
    Running,
    Finished,
    Killed,
};

[[nodiscard]] std::string toString(RunState state);

struct RunResult {
    bool success = false;
    int exitCode = -1;
    std::string output;
    std::string errorMsg;
};

class IRunBackend {
public:
    virtual ~IRunBackend() = default;

    virtual bool launch(
        const std::string& executablePath,
        const std::vector<std::string>& args,
        const std::string& workingDir,
        const std::vector<std::string>& envVars,
        std::function<void(const std::string& stream,
                            const std::string& line)> onOutput) = 0;

    virtual RunResult wait(int timeoutMs = 0) = 0;
    virtual void kill() = 0;
    [[nodiscard]] virtual bool isRunning() const = 0;
    [[nodiscard]] virtual int pid() const = 0;
};

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
    ~RunManager();

    void setBackend(std::unique_ptr<IRunBackend> backend);

    void run(const std::string& executablePath,
             const std::vector<std::string>& args = {},
             const std::string& workingDir = "",
             const std::vector<std::string>& envVars = {});

    void kill();

    [[nodiscard]] RunState currentState() const;
    [[nodiscard]] bool isRunning() const;
    [[nodiscard]] const RunResult& lastResult() const;

    // Block until the current background operation finishes.
    void waitForCompletion();

private:
    events::EventBus& eventBus_;
    std::unique_ptr<IRunBackend> backend_;
    RunState state_ = RunState::Idle;
    RunResult lastResult_;

    std::thread workThread_;
    std::atomic<bool> cancelRequested_{false};

    void transitionTo(RunState newState);
    void doRun(const std::string& executablePath,
               const std::vector<std::string>& args,
               const std::string& workingDir,
               const std::vector<std::string>& envVars);
};

} // namespace lazycmake::build
