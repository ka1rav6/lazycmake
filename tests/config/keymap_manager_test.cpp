// ==========================================================================
// KeymapManager tests
//
// Tests cover:
//   - Default keymap has expected bindings
//   - Key resolution in various contexts
//   - Action-to-key lookup
//   - Loading overrides from JSON
//   - Partial override merging
// ==========================================================================

#include <catch2/catch_test_macros.hpp>

#include "lazycmake/config/keymap_manager.hpp"

using namespace lazycmake::config;

TEST_CASE("Default keymap has vim-like bindings", "[config][keymap]") {
    KeymapManager mgr;

    // Global bindings.
    CHECK(mgr.resolve("global", "q") == "quit");
    CHECK(mgr.resolve("global", "h") == "help");
    CHECK(mgr.resolve("global", "s") == "settings");
    CHECK(mgr.resolve("global", "esc") == "close_overlay");
    CHECK(mgr.resolve("global", "ctrl+c") == "interrupt");

    // Main workspace bindings.
    CHECK(mgr.resolve("main_workspace", "b") == "build");
    CHECK(mgr.resolve("main_workspace", "r") == "run");
    CHECK(mgr.resolve("main_workspace", "c") == "configure");
    CHECK(mgr.resolve("main_workspace", "g") == "generate");
    CHECK(mgr.resolve("main_workspace", "tab") == "next_panel");
    CHECK(mgr.resolve("main_workspace", "shift+tab") == "previous_panel");
}

TEST_CASE("Unbound keys return empty action", "[config][keymap]") {
    KeymapManager mgr;

    CHECK(mgr.resolve("global", "zzzz").empty());
    CHECK(mgr.resolve("nonexistent_context", "q").empty());
}

TEST_CASE("Find key for action works in reverse", "[config][keymap]") {
    KeymapManager mgr;

    CHECK(mgr.findKey("global", "quit") == "q");
    CHECK(mgr.findKey("global", "help") == "h");
    CHECK(mgr.findKey("main_workspace", "build") == "b");
    CHECK(mgr.findKey("main_workspace", "run") == "r");
}

TEST_CASE("Loading keymap overrides from JSON works", "[config][keymap]") {
    KeymapManager mgr;

    // Override: rebind quit from 'q' to 'Q'.
    bool loaded = mgr.loadString(R"([
        {
            "context": "global",
            "bindings": [
                {"key": "Q", "action": "quit"}
            ]
        }
    ])");

    CHECK(loaded);
    CHECK(mgr.resolve("global", "Q") == "quit");

    // The old binding for 'q' should still work (or be overridden
    // depending on the merge semantics — our implementation adds
    // new bindings on top, so 'q' -> 'quit' is still there since
    // we only added 'Q' -> 'quit', not removed 'q' -> 'quit').
    // Actually, looking at setBinding: it adds OR replaces by key.
    // Since 'Q' != 'q', both exist.
    CHECK(mgr.resolve("global", "q") == "quit");  // old binding preserved
}

TEST_CASE("Build overlay has correct default bindings", "[config][keymap]") {
    KeymapManager mgr;

    CHECK(mgr.resolve("build_overlay", "b") == "build");
    CHECK(mgr.resolve("build_overlay", "c") == "configure");
    CHECK(mgr.resolve("build_overlay", "esc") == "close");
    CHECK(mgr.resolve("build_overlay", "ctrl+c") == "cancel_build");
}

TEST_CASE("Wizard has navigation bindings", "[config][keymap]") {
    KeymapManager mgr;

    CHECK(mgr.resolve("wizard", "l") == "next");
    CHECK(mgr.resolve("wizard", "h") == "previous");
    CHECK(mgr.resolve("wizard", "enter") == "select");
    CHECK(mgr.resolve("wizard", "esc") == "cancel");
}

TEST_CASE("Dialog bindings are set correctly", "[config][keymap]") {
    KeymapManager mgr;

    CHECK(mgr.resolve("dialogs", "y") == "yes");
    CHECK(mgr.resolve("dialogs", "n") == "no");
    CHECK(mgr.resolve("dialogs", "enter") == "confirm");
    CHECK(mgr.resolve("dialogs", "esc") == "cancel");
}

TEST_CASE("Get all bindings for a context returns expected count", "[config][keymap]") {
    KeymapManager mgr;

    auto globalBindings = mgr.getBindings("global");
    CHECK_FALSE(globalBindings.empty());

    // Should have at least 5 global bindings.
    CHECK(globalBindings.size() >= 5);

    // All bindings should have both key and action.
    for (const auto& b : globalBindings) {
        CHECK_FALSE(b.key.empty());
        CHECK_FALSE(b.action.empty());
    }
}

TEST_CASE("List all contexts includes all expected contexts", "[config][keymap]") {
    KeymapManager mgr;

    auto contexts = mgr.contexts();
    CHECK_FALSE(contexts.empty());

    // Check for expected context names.
    bool hasGlobal = false;
    bool hasMainWorkspace = false;
    bool hasWizard = false;
    bool hasBuildOverlay = false;

    for (const auto& ctx : contexts) {
        if (ctx == "global") hasGlobal = true;
        if (ctx == "main_workspace") hasMainWorkspace = true;
        if (ctx == "wizard") hasWizard = true;
        if (ctx == "build_overlay") hasBuildOverlay = true;
    }

    CHECK(hasGlobal);
    CHECK(hasMainWorkspace);
    CHECK(hasWizard);
    CHECK(hasBuildOverlay);
}

// End of keymap manager tests
