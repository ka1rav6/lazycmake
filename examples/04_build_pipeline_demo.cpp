// ==========================================================================
// Example 4: Build Pipeline Demo
//
// This example demonstrates the BuildManager state machine (§9.1) and
// RunManager (§9.2) with fake backends — no real subprocesses required.
//
// The build pipeline:
//   Idle → Configuring → Generating → Building → Succeeded
//                              ↓ (on error)
//                           Failed → Idle (after cancel/reset)
//
// The Run pipeline:
//   Idle → Launching → Running → Finished
//                        ↓ (on kill)
//                     Killed
//
// All progress is delivered via EventBus events, which we subscribe to
// for live display — the same way the TUI's Build Overlay would.
// ==========================================================================

#include <iostream>

#include "lazycmake/events/event_bus.hpp"
#include "lazycmake/build/build_manager.hpp"
#include "lazycmake/build/run_manager.hpp"

using namespace lazycmake::events;
using namespace lazycmake::build;

int main() {
    std::cout << "=== LazyCMake Example 4: Build Pipeline Demo ===\n\n";

    // ---- Set up EventBus with subscriber that prints events ----
    EventBus bus;

    // Subscribe to all event types and print them.
    // In the real TUI, these subscribers update the Build Overlay's
    // checklist UI and streaming output panel.
    bus.subscribe<ConfigureFinishedEvent>([](const auto& e) {
        std::cout << "  [Configure] " << (e.success ? "✅ Success" : "❌ Failed")
                  << "\n";
        if (!e.errorMsg.empty()) {
            std::cout << "    Error: " << e.errorMsg << "\n";
        }
    });

    bus.subscribe<BuildProgressEvent>([](const auto& e) {
        std::cout << "  [Progress] " << e.stage
                  << " (" << e.percentComplete << "%)"
                  << " — " << e.detail << "\n";
    });

    bus.subscribe<BuildFinishedEvent>([](const auto& e) {
        std::cout << "  [Build] " << (e.success ? "✅ Succeeded" : "❌ Failed")
                  << " (exit: " << e.exitCode << ")\n";
    });

    bus.subscribe<RunStartedEvent>([](const auto& e) {
        std::cout << "  [Run] 🚀 Starting: " << e.executablePath << "\n";
    });

    bus.subscribe<RunOutputEvent>([](const auto& e) {
        std::cout << "  [" << e.stream << "] " << e.line << "\n";
    });

    bus.subscribe<RunFinishedEvent>([](const auto& e) {
        std::cout << "  [Run] " << (e.success ? "✅ Finished" : "❌ Failed")
                  << " (exit: " << e.exitCode << ")\n";
    });

    // ======================================================================
    // Part 1: BuildManager with FakeBuildBackend (no real subprocess)
    //
    // The BuildManager orchestrates the Configure → Generate → Build
    // pipeline. FakeBuildBackend simulates success or failure without
    // actually running CMake or Ninja, making it perfect for testing
    // the state machine logic.
    // ======================================================================

    std::cout << "--- Part 1: Successful Build ---\n\n";

    auto successfulBackend = std::make_unique<FakeBuildBackend>(/*shouldSucceed=*/true);
    BuildManager buildManager(std::move(successfulBackend), bus);

    std::cout << "  Running configure...\n";
    buildManager.configure("/project", "/build", "Ninja", "Debug");
    std::cout << "  Stage: " << toString(buildManager.currentStage()) << "\n\n";

    std::cout << "  Running build...\n";
    buildManager.build("my_app");
    std::cout << "  Stage: " << toString(buildManager.currentStage()) << "\n";
    std::cout << "  Last result: "
              << (buildManager.lastResult().status == BuildOperationStatus::Completed
                      ? "Completed" : "Failed")
              << " (exit: " << buildManager.lastResult().exitCode << ")\n\n";

    // ======================================================================
    // Part 2: Build failure
    //
    // With shouldSucceed=false, the FakeBuildBackend simulates a build
    // failure, letting us test the error path.
    // ======================================================================

    std::cout << "--- Part 2: Failed Build ---\n\n";

    auto failingBackend = std::make_unique<FakeBuildBackend>(/*shouldSucceed=*/false);
    buildManager.setBackend(std::move(failingBackend));

    buildManager.build("broken_target");
    std::cout << "  Stage: " << toString(buildManager.currentStage()) << "\n";
    std::cout << "  Last result status: ";
    switch (buildManager.lastResult().status) {
        case BuildOperationStatus::Completed: std::cout << "Completed"; break;
        case BuildOperationStatus::Failed: std::cout << "Failed"; break;
        case BuildOperationStatus::Cancelled: std::cout << "Cancelled"; break;
        default: std::cout << "Other"; break;
    }
    std::cout << "\n\n";

    // Reset for the next part.
    buildManager.reset();
    std::cout << "  After reset, stage: " << toString(buildManager.currentStage()) << "\n\n";

    // ======================================================================
    // Part 3: RunManager — launching an executable
    //
    // The RunManager launches a built executable and streams its output
    // via EventBus. FakeRunBackend simulates the process without actually
    // running anything.
    // ======================================================================

    std::cout << "--- Part 3: Run Executable ---\n\n";

    RunManager runManager(bus);

    // Use a fake backend that succeeds with exit code 0.
    runManager.setBackend(std::make_unique<FakeRunBackend>(/*shouldSucceed=*/true, /*exitCode=*/0));
    runManager.run("/usr/bin/my_app", {"--verbose", "--config=debug"}, "/tmp", {});

    std::cout << "\n  Final state: " << toString(runManager.currentState()) << "\n";
    std::cout << "  Result: " << (runManager.lastResult().success ? "✅" : "❌")
              << " (exit: " << runManager.lastResult().exitCode << ")\n\n";

    // ======================================================================
    // Part 4: Run failure
    // ======================================================================

    std::cout << "--- Part 4: Run Failure ---\n\n";

    runManager.setBackend(std::make_unique<FakeRunBackend>(/*shouldSucceed=*/false, /*exitCode=*/1));
    runManager.run("/usr/bin/failing_tool");

    std::cout << "\n  Result: " << (runManager.lastResult().success ? "✅" : "❌")
              << " (exit: " << runManager.lastResult().exitCode << ")\n";
    std::cout << "  Error: " << runManager.lastResult().errorMsg << "\n\n";

    // ======================================================================
    // Part 5: Kill a running process
    // ======================================================================

    std::cout << "--- Part 5: Kill Running Process ---\n\n";

    auto killableBackend = std::make_unique<FakeRunBackend>(/*shouldSucceed=*/true, /*exitCode=*/0);
    FakeRunBackend* backendPtr = killableBackend.get();

    runManager.setBackend(std::move(killableBackend));
    runManager.run("/usr/bin/long_running_task");

    std::cout << "  Process has PID: " << backendPtr->pid() << "\n";
    std::cout << "  Launch parameters:\n";
    std::cout << "    executable: " << backendPtr->lastExecutable() << "\n";
    std::cout << "    args (" << backendPtr->lastArgs().size() << "): ";
    for (const auto& arg : backendPtr->lastArgs()) {
        std::cout << "'" << arg << "' ";
    }
    std::cout << "\n\n";

    std::cout << "=== Done! ===\n";
    return 0;
}
