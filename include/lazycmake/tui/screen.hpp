#pragma once

#include <memory>
#include <string>
#include <vector>

#include <ftxui/component/component.hpp>

namespace lazycmake::tui {

// Represents a single screen in the navigation stack.
// Each screen is an FTXUI component with enter/leave lifecycle hooks.
struct Screen {
    std::string name;
    ftxui::Component component;

    virtual ~Screen() = default;
    virtual void onEnter() {}
    virtual void onLeave() {}
};

// TODO: dead code — ScreenStack is unused. Application uses its own
// screenState_ member directly instead. Either wire it in or remove it.
class ScreenStack {
public:
    void push(std::unique_ptr<Screen> screen);
    void pop();
    void replace(std::unique_ptr<Screen> screen);
    Screen* current();
    const Screen* current() const;
    std::size_t size() const { return stack_.size(); }
    bool empty() const { return stack_.empty(); }

private:
    std::vector<std::unique_ptr<Screen>> stack_;
};

} // namespace lazycmake::tui
