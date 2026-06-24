#include "lazycmake/events/event_bus.hpp"

#include <algorithm>
#include <iostream>

namespace lazycmake::events {

EventBus::EventBus() = default;
EventBus::~EventBus() = default;

void EventBus::unsubscribe(HandlerId id) {
    std::erase_if(handlers_, [id](const Handler& h) { return h.id == id; });
}

void EventBus::publish(const Event& event) {
    dispatch(event);
}

void EventBus::publishThreadSafe(const Event& event) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    threadSafeQueue_.push(event);
}

void EventBus::drainQueue() {
    // Swap the queue so we minimize lock contention.
    std::queue<Event> localQueue;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        localQueue.swap(threadSafeQueue_);
    }

    // Deliver all events on the calling thread.
    while (!localQueue.empty()) {
        dispatch(localQueue.front());
        localQueue.pop();
    }
}

std::size_t EventBus::pendingCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return threadSafeQueue_.size();
}

void EventBus::clear() {
    handlers_.clear();
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        while (!threadSafeQueue_.empty()) {
            threadSafeQueue_.pop();
        }
    }
}

void EventBus::dispatch(const Event& event) {
    // Iterate over handlers and call those that match the event type.
    // We match by variant index (computed at subscribe time) rather than
    // trying std::get which would throw on mismatch.
    std::size_t eventIdx = event.index();

    for (const auto& handler : handlers_) {
        if (handler.eventTypeIndex == eventIdx) {
            try {
                handler.handler(event);
            } catch (const std::exception& e) {
                std::cerr << "EventBus: Handler " << handler.id
                          << " threw: " << e.what() << std::endl;
            }
        }
    }
}

} // namespace lazycmake::events
