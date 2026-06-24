// ==========================================================================
// EventBus tests
//
// Tests cover:
//   - Subscribe and publish basic events
//   - Type-safe dispatch (only matching handlers are called)
//   - Multiple subscribers for the same event
//   - Unsubscribe
//   - Thread-safe publish and drain queue
//   - PluginEventBus facade
//   - Handler exception safety
// ==========================================================================

#include <catch2/catch_test_macros.hpp>

#include "lazycmake/events/event_bus.hpp"

using namespace lazycmake::events;

TEST_CASE("Subscribe and publish a single event type", "[events][event_bus]") {
    EventBus bus;

    bool received = false;
    bus.subscribe<BuildStartedEvent>([&](const BuildStartedEvent& e) {
        received = true;
        CHECK(e.targetName == "my_app");
    });

    bus.publish(BuildStartedEvent{.targetName = "my_app"});

    CHECK(received);
}

TEST_CASE("Only matching event types trigger their handlers", "[events][event_bus]") {
    EventBus bus;

    int buildStartedCount = 0;
    int buildFinishedCount = 0;

    bus.subscribe<BuildStartedEvent>([&](const auto&) { ++buildStartedCount; });
    bus.subscribe<BuildFinishedEvent>([&](const auto&) { ++buildFinishedCount; });

    bus.publish(BuildStartedEvent{.targetName = "app"});
    bus.publish(BuildStartedEvent{.targetName = "lib"});
    bus.publish(BuildFinishedEvent{.success = true, .targetName = "app"});

    CHECK(buildStartedCount == 2);
    CHECK(buildFinishedCount == 1);
}

TEST_CASE("Multiple subscribers for the same event all receive it", "[events][event_bus]") {
    EventBus bus;

    int callCount = 0;
    auto id1 = bus.subscribe<BuildProgressEvent>([&](const auto&) { ++callCount; });
    auto id2 = bus.subscribe<BuildProgressEvent>([&](const auto&) { ++callCount; });

    bus.publish(BuildProgressEvent{.stage = "Building", .percentComplete = 50});

    CHECK(callCount == 2);

    // Unsubscribe one handler.
    bus.unsubscribe(id1);

    bus.publish(BuildProgressEvent{.stage = "Linking", .percentComplete = 90});

    CHECK(callCount == 3);  // only id2 was called
}

TEST_CASE("Unsubscribe removes the handler permanently", "[events][event_bus]") {
    EventBus bus;

    int callCount = 0;
    auto id = bus.subscribe<ProjectLoadedEvent>([&](const auto&) { ++callCount; });

    bus.publish(ProjectLoadedEvent{.projectName = "Test"});
    CHECK(callCount == 1);

    bus.unsubscribe(id);

    bus.publish(ProjectLoadedEvent{.projectName = "Test2"});
    CHECK(callCount == 1);  // no change
}

TEST_CASE("Thread-safe queue delivers events on drain", "[events][event_bus]") {
    EventBus bus;

    int receivedCount = 0;
    bus.subscribe<RunOutputEvent>([&](const auto&) { ++receivedCount; });

    // Publish from "background thread" (simulated here on the same thread).
    bus.publishThreadSafe(RunOutputEvent{.stream = "stdout", .line = "line1"});
    bus.publishThreadSafe(RunOutputEvent{.stream = "stdout", .line = "line2"});
    bus.publishThreadSafe(RunOutputEvent{.stream = "stderr", .line = "error1"});

    CHECK(receivedCount == 0);  // not yet delivered

    CHECK(bus.pendingCount() == 3);

    bus.drainQueue();

    CHECK(receivedCount == 3);
    CHECK(bus.pendingCount() == 0);
}

TEST_CASE("EventBus clear removes all handlers and queued events", "[events][event_bus]") {
    EventBus bus;

    int callCount = 0;
    bus.subscribe<ConfigReloadedEvent>([&](const auto&) { ++callCount; });

    bus.publishThreadSafe(ConfigReloadedEvent{.configType = "theme"});
    bus.clear();

    // After clear, no handlers should receive events.
    bus.drainQueue();
    CHECK(callCount == 0);

    // And new publishes should also not be received.
    bus.publish(ConfigReloadedEvent{.configType = "keymap"});
    CHECK(callCount == 0);
}

TEST_CASE("PluginEventBus allows subscribe but not publish", "[events][event_bus]") {
    EventBus bus;

    // Create a plugin facade.
    PluginEventBus pluginBus(bus);

    int receivedCount = 0;
    pluginBus.subscribe<FileAddedEvent>([&](const auto&) { ++receivedCount; });

    // The real bus publishes.
    bus.publish(FileAddedEvent{.path = "src/new_file.cpp"});

    CHECK(receivedCount == 1);

    // PluginEventBus has no publish() method — this is enforced at
    // compile time by the class definition.
}

TEST_CASE("Different event types carry their specific data", "[events][event_bus]") {
    EventBus bus;

    // Test several event types to verify data is correctly transmitted.

    bus.subscribe<BuildFinishedEvent>([&](const BuildFinishedEvent& e) {
        CHECK(e.success == true);
        CHECK(e.targetName == "app");
        CHECK(e.exitCode == 0);
    });

    bus.subscribe<RunOutputEvent>([&](const RunOutputEvent& e) {
        CHECK(e.stream == "stdout");
        CHECK(e.line == "Hello, World!");
    });

    bus.subscribe<ConfigReloadedEvent>([&](const ConfigReloadedEvent& e) {
        CHECK(e.configType == "settings");
    });

    bus.publish(BuildFinishedEvent{
        .success = true,
        .targetName = "app",
        .exitCode = 0,
        .output = "Build succeeded",
    });

    bus.publish(RunOutputEvent{
        .stream = "stdout",
        .line = "Hello, World!",
    });

    bus.publish(ConfigReloadedEvent{
        .configType = "settings",
    });
}

TEST_CASE("DrainQueue is safe to call with no pending events", "[events][event_bus]") {
    EventBus bus;

    // Should not crash or throw.
    bus.drainQueue();
    bus.drainQueue();
}

TEST_CASE("Handler exceptions are caught and don't crash the bus", "[events][event_bus]") {
    EventBus bus;

    // Handler that throws.
    bus.subscribe<BuildStartedEvent>([&](const auto&) {
        throw std::runtime_error("handler error");
    });

    // Second handler that should still be called.
    bool secondCalled = false;
    bus.subscribe<BuildStartedEvent>([&](const auto&) {
        secondCalled = true;
    });

    // Should not crash.
    bus.publish(BuildStartedEvent{.targetName = "app"});

    CHECK(secondCalled);
}

// End of event bus tests
