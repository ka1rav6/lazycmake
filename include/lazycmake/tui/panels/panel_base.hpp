#pragma once

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

namespace lazycmake::tui {

// PanelBase — abstract base class for all TUI panels (FilePanel, TargetPanel, OutputPanel)
// Defines the interface for panel operations:
//   - Component building for rendering
//   - Focus management (focus/blur) for keyboard navigation
//   - Event handling for user input
//   - Visibility control (show/hide/toggle/isVisible) for overlay management
// 
// Each concrete panel must implement all pure virtual methods and provide a
// virtual destructor to ensure proper cleanup.
// 
// This design enables:
//   - Consistent behavior across all panels
//   - Proper memory management for polymorphic objects
//   - Clean separation between panel lifecycle and UI rendering
struct PanelBase {
    // Virtual destructor ensures proper cleanup when deleting derived objects
    // through a base class pointer. Using = default avoids code duplication
    // while ensuring correct cleanup behavior.
    virtual ~PanelBase() = default;
    
    // Create and return the FTXUI component that will be rendered for this panel.
    // This method is responsible for all visual elements and layout.
    // Concrete implementations must build and return the FTXUI component
    // that represents this panel in the TUI.
    virtual ftxui::Component build() = 0;
    
    // Mark this panel as focused for keyboard input and user interaction.
    // Called when navigation moves to this panel, typically via Tab key.
    // Focus allows this panel to receive keyboard events.
    virtual void focus() = 0;
    
    // Remove focus from this panel, typically when navigation moves away.
    // Other panels can accept keyboard input when this panel is blurred.
    // Usually paired with a corresponding focus() call on another panel.
    virtual void blur() = 0;
    
    // Process keyboard and other events for this panel.
    // Returns true if the event was consumed and handled, false to pass it on.
    // This enables panels to accept/reject events based on current state,
    // panel focus, and UI requirements (e.g., input fields vs navigation).
    virtual bool onEvent(ftxui::Event event) = 0;
    
    // Make the panel visible to the user. Called when panel should become
    // active and visible in the UI. Typically triggers a redraw to show
    // the panel's contents to the user.
    virtual void show() = 0;
    
    // Hide the panel from the user. Called when panel should be obscured
    // or deactivated. Typically triggers a redraw to hide UI elements
    // when panel visibility changes.
    virtual void hide() = 0;
    
    // Toggle visibility: if visible, hide it; if hidden, show it.
    // Useful for creating overlay-like behavior (e.g., build overlay, run overlay).
    // Commonly used with dialogs and temporary UI elements.
    virtual void toggle() = 0;
    
    // Check if the panel is currently visible and should be rendered.
    // Returns true if the panel should be rendered in the UI, false if it's
    // hidden or should be obscured. Used by overlays and UI logic to decide
    // whether to draw this panel.
    virtual bool isVisible() const = 0;
};

inline namespace details {

// Deletor function object for use with smart pointers.
// Enables using std::unique_ptr with PanelBase-derived types while ensuring
// proper cleanup of polymorphic objects.
//
// Example usage:
//   std::unique_ptr<PanelBase, PanelBaseDeleter> panel(new FilePanel(...));
struct PanelBaseDeleter {
    // Delete the PanelBase pointer and all derived objects.
    // Safe for use with unique_ptr to ensure consistent cleanup.
    void operator()(PanelBase* ptr) const {
        delete ptr;
    }
};

} // namespace details

} // namespace lazycmake::tui
