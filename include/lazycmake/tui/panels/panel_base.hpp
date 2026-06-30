#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

struct PanelBase {
    virtual ~PanelBase() = default;
    virtual ftxui::Component build() = 0;
    virtual void focus() = 0;
    virtual void blur() = 0;
    virtual bool onEvent(ftxui::Event event) = 0;
};

} // namespace lazycmake::tui
