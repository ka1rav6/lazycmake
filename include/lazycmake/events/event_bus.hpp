#pragma once

// ==========================================================================
// EventBus — typed pub/sub event system for cross-layer communication (§8)
//
// The EventBus is the backbone of all cross-layer communication in
// LazyCMake. The TUI layer, build engine, file watcher, plugins, and
// configuration system all communicate through this bus rather than
// through direct method calls or scattered callbacks.
//
// Key design decisions (§8):
//
//   - Typed variant, not stringly-typed: using std::variant means
//     subscribers get compile-time checking of event types. A plugin
//     author gets a compiler error, not a runtime surprise.
//
//   - Single-threaded delivery to the UI: background threads (build
//     process, file watcher) only call publishThreadSafe(), which pushes
//     onto a lock-protected queue. Application::tick() drains it on the
//     main thread before each FTXUI render pass.
//
//   - Plugin facade: PluginHost exposes a restricted PluginEventBus that
//     only allows subscribing, not publishing arbitrary core events.
//     A plugin can REACT to a build finishing but cannot FORGE a
//     BuildFinishedEvent.
//
// Event types (add more as needed):
//   ProjectLoadedEvent / ProjectSavedEvent
//   ConfigureStartedEvent / ConfigureFinishedEvent
//   BuildStartedEvent / BuildProgressEvent / BuildFinishedEvent
//   RunStartedEvent / RunOutputEvent / RunFinishedEvent
//   FileAddedEvent / FileRemovedEvent / FileRenamedEvent
//   DependencyToggledEvent
//   ConfigReloadedEvent
//   PluginLoadedEvent / PluginErrorEvent
// ==========================================================================

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "lazycmake/core/project.hpp"

namespace lazycmake::events {

// ==========================================================================
// Event type definitions
// Each is a simple struct with the data relevant to that event.
// ==========================================================================

struct ProjectLoadedEvent {
    std::string projectName;
    std::string projectPath;
};

struct ProjectSavedEvent {
    std::string projectName;
    std::string projectPath;
};

struct ConfigureStartedEvent {
    std::string buildDir;
};

struct ConfigureFinishedEvent {
    bool success;
    std::string output;     // captured CMake configure output
    std::string errorMsg;   // error message if !success
};

struct BuildStartedEvent {
    std::string targetName;
};

struct BuildProgressEvent {
    std::string stage;      // "Configuring", "Generating", "Building", "Linking"
    int percentComplete;    // 0-100, or -1 if unknown
    std::string detail;     // e.g. "Building engine.cpp.o"
};

struct BuildFinishedEvent {
    bool success;
    std::string targetName;
    int exitCode;
    std::string output;
};

struct RunStartedEvent {
    std::string targetName;
    std::string executablePath;
};

struct RunOutputEvent {
    std::string stream;     // "stdout" or "stderr"
    std::string line;
};

struct RunFinishedEvent {
    bool success;
    int exitCode;
};

struct FileAddedEvent {
    std::string path;
};

struct FileRemovedEvent {
    std::string path;
};

struct FileRenamedEvent {
    std::string oldPath;
    std::string newPath;
};

struct DependencyToggledEvent {
    std::string dependencyName;
    bool nowEnabled;
};

struct ConfigReloadedEvent {
    std::string configType;  // "settings", "keymap", "theme", "templates"
};

struct PluginLoadedEvent {
    std::string pluginName;
    std::string pluginVersion;
};

struct PluginErrorEvent {
    std::string pluginName;
    std::string errorMessage;
};

// The complete set of event types as a variant.
// Adding a new event type means:
//   1. Define the struct above.
//   2. Add it to this variant type.
//   3. Subscribe to it with eventBus.subscribe<MyNewEvent>(handler).
using Event = std::variant<
    ProjectLoadedEvent,
    ProjectSavedEvent,
    ConfigureStartedEvent,
    ConfigureFinishedEvent,
    BuildStartedEvent,
    BuildProgressEvent,
    BuildFinishedEvent,
    RunStartedEvent,
    RunOutputEvent,
    RunFinishedEvent,
    FileAddedEvent,
    FileRemovedEvent,
    FileRenamedEvent,
    DependencyToggledEvent,
    ConfigReloadedEvent,
    PluginLoadedEvent,
    PluginErrorEvent
>;

// ==========================================================================
// EventBus — the main event bus class
//
// Thread-safety: subscribers are called on the thread that calls
// publish() or drainQueue(). The publishThreadSafe() method is the
// only thread-safe entry point — it queues the event for delivery
// on the main thread during the next drainQueue() call.
// ==========================================================================

class EventBus {
public:
    using HandlerId = std::uint64_t;

    EventBus();
    ~EventBus();

    // Subscribe to a specific event type. The handler will be called
    // for every event of type T published via publish() or drainQueue().
    // Returns a HandlerId that can be used to unsubscribe.
    //
    // Usage:
    //   auto id = bus.subscribe<BuildFinishedEvent>([](const auto& e) {
    //       fmt::print("Build {}: {}\n", e.targetName,
    //                  e.success ? "succeeded" : "failed");
    //   });
    template <typename T>
    HandlerId subscribe(std::function<void(const T&)> handler);

    // Unsubscribe a previously registered handler.
    void unsubscribe(HandlerId id);

    // Publish an event synchronously on the calling thread.
    // All matching subscribers are called immediately, in registration
    // order. This is the main dispatch path for events originating on
    // the UI thread.
    void publish(const Event& event);

    // Publish an event from any thread. The event is queued and will
    // be delivered during the next drainQueue() call on the main thread.
    // This is the dispatch path for events from background threads
    // (build process output, file watcher notifications).
    void publishThreadSafe(const Event& event);

    // Drain the thread-safe queue, delivering all queued events to
    // subscribers on the calling thread. Called once per frame by
    // Application::tick() before the FTXUI render pass.
    void drainQueue();

    // Get the number of queued (undrained) thread-safe events.
    [[nodiscard]] std::size_t pendingCount() const;

    // Remove all subscribers and clear the queue.
    void clear();

private:
    // Internal handler storage: a type-erased function + the subscribed
    // event type index (for dispatch matching).
    struct Handler {
        HandlerId id;
        std::size_t eventTypeIndex;  // type_index of the Event variant alternative
        std::function<void(const Event&)> handler;  // type-erased wrapper
    };

    std::vector<Handler> handlers_;
    HandlerId nextHandlerId_ = 1;

    // Thread-safe queue for events from background threads.
    mutable std::mutex queueMutex_;
    std::queue<Event> threadSafeQueue_;

    // Dispatch a single event to all matching subscribers.
    void dispatch(const Event& event);

    // Callback invoked after each publishThreadSafe() to wake the UI.
    // Set by Application to post a custom event to the FTXUI screen.
    std::function<void()> onEventQueued_;

public:
    // Set a callback that is invoked (on the calling thread) after every
    // publishThreadSafe() call. Used to wake the FTXUI event loop so it
    // drains the queue and repaints.
    void setOnEventQueued(std::function<void()> cb) { onEventQueued_ = std::move(cb); }
};

// ==========================================================================
// Template implementation of subscribe<T>
// ==========================================================================

template <typename T>
EventBus::HandlerId EventBus::subscribe(std::function<void(const T&)> handler) {
    // Wrap the typed handler in a type-erased lambda that extracts the
    // correct alternative from the Event variant and calls the handler.
    // If the event is not of type T, the dispatch loop skips it based
    // on the eventTypeIndex comparison.
    HandlerId id = nextHandlerId_++;

    // Get the index of T in the Event variant.
    // We capture this at subscription time so dispatch doesn't need to
    // try std::get for every handler — it checks the index first.
    constexpr std::size_t typeIndex = []() {
        // Find the index of T in the variant types list.
        // This is a constexpr lambda that computes the index at compile time.
        std::size_t idx = 0;
        bool found = false;
        // We use a fold expression trick: iterate over the variant's types.
        // C++20 doesn't have std::variant_index, so we use this approach.
        // The types are visited via a helper.
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ((found ? false : (std::is_same_v<
                std::variant_alternative_t<Is, Event>, T> ? (idx = Is, found = true) : false)), ...);
        }(std::make_index_sequence<std::variant_size_v<Event>>());
        return idx;
    }();

    handlers_.push_back(Handler{
        .id = id,
        .eventTypeIndex = typeIndex,
        .handler = [handler = std::move(handler)](const Event& event) {
            handler(std::get<T>(event));
        },
    });

    return id;
}

// ==========================================================================
// PluginEventBus — restricted facade for plugins (§8, §16.2)
//
// Plugins can only subscribe to events and query pending count.
// They cannot publish core events (which would let a plugin pretend
// to be the build engine and forge a BuildFinishedEvent).
//
// The PluginHost creates one of these per loaded plugin and passes
// it in PluginContext.
// ==========================================================================

class PluginEventBus {
public:
    explicit PluginEventBus(EventBus& bus)
        : bus_(bus) {}

    // Subscribe to an event type (same as EventBus::subscribe).
    template <typename T>
    EventBus::HandlerId subscribe(std::function<void(const T&)> handler) {
        return bus_.subscribe<T>(std::move(handler));
    }

    // Unsubscribe a handler.
    void unsubscribe(EventBus::HandlerId id) {
        bus_.unsubscribe(id);
    }

    // Check pending count (read-only query).
    [[nodiscard]] std::size_t pendingCount() const {
        return bus_.pendingCount();
    }

    // Not provided: publish(), publishThreadSafe(), drainQueue(), clear().

private:
    EventBus& bus_;
};

} // namespace lazycmake::events
