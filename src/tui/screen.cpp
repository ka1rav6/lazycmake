#include "lazycmake/tui/screen.hpp"

namespace lazycmake::tui {

void ScreenStack::push(std::unique_ptr<Screen> screen) {
    if (screen) {
        screen->onEnter();
        stack_.push_back(std::move(screen));
    }
}

void ScreenStack::pop() {
    if (!stack_.empty()) {
        auto* old = stack_.back().get();
        if (old) old->onLeave();
        stack_.pop_back();
        if (!stack_.empty()) {
            stack_.back()->onEnter();
        }
    }
}

void ScreenStack::replace(std::unique_ptr<Screen> screen) {
    if (!stack_.empty()) {
        stack_.back()->onLeave();
        stack_.pop_back();
    }
    if (screen) {
        screen->onEnter();
        stack_.push_back(std::move(screen));
    }
}

Screen* ScreenStack::current() {
    if (stack_.empty()) return nullptr;
    return stack_.back().get();
}

const Screen* ScreenStack::current() const {
    if (stack_.empty()) return nullptr;
    return stack_.back().get();
}

} // namespace lazycmake::tui
