// ==========================================================================
// Example 2: EventBus Usage Patterns
//
// This example demonstrates the EventBus (§8), the backbone of all
// cross-layer communication in LazyCMake. You'll see:
//   1. How to subscribe to events
//   2. How to publish events
//   3. Thread-safe publishing from "background threads"
//   4. The PluginEventBus restricted facade
//   5. Unsubscribing handlers
//
// The EventBus uses a typed std::variant approach: you subscribe to
// specific event types and get compile-time type checking. There's
// no string-based event names or void* payloads.
//
// To build: (similar to example 1, include src/events/event_bus.cpp)
// ==========================================================================

#include <iostream>
#include <thread>
#include <chrono>

#include "lazycmake/events/event_bus.hpp"

using namespace lazycmake::events;

int main() {
    std::cout << "=== LazyCMake Example 2: EventBus Usage ===\n\n";

    // ======================================================================
    // Step 1: Create the EventBus
    //
    // The EventBus is the central communication hub. There's typically
    // one bus per Application instance, owned by the Application class.
    // ======================================================================

    EventBus bus;

    // ======================================================================
    // Step 2: Subscribe to events
    //
    // Use subscribe<T>() with a lambda or function. The template parameter
    // selects which event type to receive. Other event types are silently
    // skipped during dispatch.
    //
    // subscribe() returns a HandlerId that can be used to unsubscribe later.
    // ======================================================================

    // Subscribe to all build progress events.
    auto progressId = bus.subscribe<BuildProgressEvent>(
        [](const BuildProgressEvent& e) {
            std::cout << "  [BuildProgress] " << e.stage
                      << " " << e.percentComplete << "%"
                      << " — " << e.detail << "\n";
        });

    // Subscribe to build finished events with different handlers.
    bus.subscribe<BuildFinishedEvent>([](const BuildFinishedEvent& e) {
        if (e.success) {
            std::cout << "  ✅ Build of '" << e.targetName << "' succeeded"
                      << " (exit code: " << e.exitCode << ")\n";
        } else {
            std::cout << "  ❌ Build of '" << e.targetName << "' FAILED"
                      << " (exit code: " << e.exitCode << ")\n";
        }
    });

    // Subscribe to run output events (streaming stdout/stderr).
    bus.subscribe<RunOutputEvent>([](const RunOutputEvent& e) {
        std::cout << "  [" << e.stream << "] " << e.line << "\n";
    });

    // Subscribe to project lifecycle events.
    bus.subscribe<ProjectLoadedEvent>([](const ProjectLoadedEvent& e) {
        std::cout << "  📂 Project loaded: " << e.projectName
                  << " from " << e.projectPath << "\n";
    });

    // ======================================================================
    // Step 3: Publish events synchronously
    //
    // All matching subscribers are called immediately, in registration
    // order. This is the main dispatch path for events originating on
    // the UI thread.
    // ======================================================================

    std::cout << "\n--- Publishing events synchronously ---\n";

    bus.publish(ProjectLoadedEvent{
        .projectName = "MyGame",
        .projectPath = "/home/user/projects/my_game",
    });

    bus.publish(BuildProgressEvent{
        .stage = "Configuring",
        .percentComplete = 10,
        .detail = "Running cmake...",
    });

    bus.publish(BuildProgressEvent{
        .stage = "Building",
        .percentComplete = 45,
        .detail = "Compiling src/Scene.cpp",
    });

    bus.publish(BuildFinishedEvent{
        .success = true,
        .targetName = "app",
        .exitCode = 0,
        .output = "Build succeeded",
    });

    // ======================================================================
    // Step 4: Thread-safe publishing
    //
    // Background threads (build process, file watcher) must use
    // publishThreadSafe(), which pushes the event onto a lock-protected
    // queue. The main thread calls drainQueue() to deliver them.
    //
    // This ensures that subscribers (including plugins) never need their
    // own locking — all handler code runs on the main thread.
    // ======================================================================

    std::cout << "\n--- Thread-safe publishing (simulated background thread) ---\n";

    // Simulate a background thread producing events.
    std::thread backgroundThread([&bus]() {
        // In a real scenario, these would be build output lines from reproc
        // or file system events from efsw. The thread only calls
        // publishThreadSafe() — it never touches the handler list directly.
        bus.publishThreadSafe(RunOutputEvent{
            .stream = "stdout",
            .line = "Hello from background thread!",
        });
        bus.publishThreadSafe(RunOutputEvent{
            .stream = "stdout",
            .line = "This is line 2 of output.",
        });
        bus.publishThreadSafe(RunOutputEvent{
            .stream = "stderr",
            .line = "Warning: this is a simulated warning.",
        });
    });

    backgroundThread.join();  // Wait for the thread to finish.

    // Events are queued but not yet delivered. Check pending count.
    std::cout << "  Pending events in queue: " << bus.pendingCount() << "\n";

    // Drain the queue on the "main thread". This delivers all queued
    // events to subscribers, calling their handlers synchronously.
    std::cout << "  Draining queue...\n";
    bus.drainQueue();
    std::cout << "  Pending events after drain: " << bus.pendingCount() << "\n";

    // ======================================================================
    // Step 5: The PluginEventBus facade
    //
    // Plugins don't get access to the full EventBus. Instead, the
    // PluginHost creates a PluginEventBus that only allows subscribing
    // (and querying pending count). A plugin cannot publish core events
    // — it cannot pretend to be the build engine and forge a
    // BuildFinishedEvent.
    //
    // This is enforced at compile time: PluginEventBus has no publish()
    // or publishThreadSafe() methods.
    // ======================================================================

    std::cout << "\n--- PluginEventBus (restricted facade) ---\n";

    PluginEventBus pluginBus(bus);  // Created by PluginHost, passed to plugin.

    // A plugin can subscribe (same as the real bus).
    int pluginReceived = 0;
    pluginBus.subscribe<ConfigReloadedEvent>([&pluginReceived](const auto&) {
        ++pluginReceived;
        std::cout << "  🔌 Plugin received ConfigReloadedEvent\n";
    });

    // The real bus publishes — the plugin handler fires.
    bus.publish(ConfigReloadedEvent{.configType = "theme"});
    std::cout << "  Plugin received " << pluginReceived << " event(s)\n";

    // The following would NOT compile — PluginEventBus has no publish():
    // pluginBus.publish(BuildFinishedEvent{...});  // COMPILE ERROR ✓

    // ======================================================================
    // Step 6: Unsubscribing
    //
    // Use the HandlerId returned by subscribe() to unsubscribe.
    // After unsubscribing, the handler will no longer be called.
    // ======================================================================

    std::cout << "\n--- Unsubscribing ---\n";

    // Unsubscribe the progress handler (we subscribed at the top).
    bus.unsubscribe(progressId);

    // Now this event won't print a progress message (handler was removed).
    std::cout << "  Publishing progress after unsubscribe (should be silent):\n";
    bus.publish(BuildProgressEvent{
        .stage = "Linking",
        .percentComplete = 90,
        .detail = "Linking final binary",
    });
    std::cout << "  (No progress output above — handler was removed)\n";

    // ======================================================================
    // Step 7: Clear the bus (removes all handlers and queued events)
    //
    // Useful during shutdown or when resetting the application state.
    // ======================================================================

    bus.clear();
    std::cout << "\n  Bus cleared. All handlers removed.\n";

    std::cout << "\n=== Done! ===\n";
    return 0;
}
