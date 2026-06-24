// ==========================================================================
// SettingsManager tests
//
// Tests cover:
//   - Default values (no config files loaded)
//   - Loading from JSON strings (CLI overrides)
//   - Layered merging (partial overrides)
//   - Reset to defaults
// ==========================================================================

#include <catch2/catch_test_macros.hpp>

#include "lazycmake/config/settings_manager.hpp"

using namespace lazycmake::config;

TEST_CASE("Default settings have expected values", "[config][settings]") {
    SettingsManager mgr;

    // Default build settings.
    CHECK(mgr.build().parallelJobs == 0);          // 0 = auto
    CHECK(mgr.build().exportCompileCommands == true);
    CHECK(mgr.build().autoConfigure == true);
    CHECK(mgr.build().verboseBuild == false);

    // Default general settings.
    CHECK(mgr.general().language == "en");
    CHECK(mgr.general().restoreLastProject == true);
    CHECK(mgr.general().confirmDestructiveActions == true);

    // Default editor settings.
    CHECK(mgr.editor().editorCommand.empty());

    // The settings manager should produce valid JSON.
    nlohmann::json j = mgr.settings();
    CHECK(j.contains("general"));
    CHECK(j.contains("build"));
    CHECK(j.contains("editor"));
}

TEST_CASE("Settings can be overridden via JSON string", "[config][settings]") {
    SettingsManager mgr;

    // Override one build setting.
    bool loaded = mgr.loadString(R"({
        "build": {
            "parallelJobs": 8,
            "verboseBuild": true
        }
    })");

    CHECK(loaded);

    // The overridden values should take effect.
    CHECK(mgr.build().parallelJobs == 8);
    CHECK(mgr.build().verboseBuild == true);

    // Non-overridden values should keep their defaults.
    CHECK(mgr.build().exportCompileCommands == true);
    CHECK(mgr.build().autoConfigure == true);

    // Unrelated sections should be unchanged.
    CHECK(mgr.general().language == "en");
}

TEST_CASE("Settings deep-merge works for nested objects", "[config][settings]") {
    SettingsManager mgr;

    // Only override one field deep in the hierarchy.
    bool loaded = mgr.loadString(R"({
        "general": {
            "language": "de"
        }
    })");

    CHECK(loaded);
    CHECK(mgr.general().language == "de");

    // Other general fields should remain at defaults.
    CHECK(mgr.general().restoreLastProject == true);
    CHECK(mgr.general().confirmDestructiveActions == true);
}

TEST_CASE("Multiple layered overrides merge correctly", "[config][settings]") {
    SettingsManager mgr;

    // Layer 1: set build.parallelJobs to 4.
    mgr.loadString(R"({"build": {"parallelJobs": 4}})");
    CHECK(mgr.build().parallelJobs == 4);

    // Layer 2: override verboseBuild (parallelJobs should stay 4).
    mgr.loadString(R"({"build": {"verboseBuild": true}})");
    CHECK(mgr.build().parallelJobs == 4);   // preserved from layer 1
    CHECK(mgr.build().verboseBuild == true); // from layer 2

    // Layer 3: override parallelJobs again.
    mgr.loadString(R"({"build": {"parallelJobs": 2}})");
    CHECK(mgr.build().parallelJobs == 2);    // overridden
    CHECK(mgr.build().verboseBuild == true); // preserved from layer 2
}

TEST_CASE("Reset to defaults clears all overrides", "[config][settings]") {
    SettingsManager mgr;

    mgr.loadString(R"({"build": {"parallelJobs": 16}})");
    CHECK(mgr.build().parallelJobs == 16);

    mgr.resetToDefaults();
    CHECK(mgr.build().parallelJobs == 0);    // back to default
    CHECK(mgr.build().verboseBuild == false);
}

TEST_CASE("Invalid JSON string returns false and does not change settings", "[config][settings]") {
    SettingsManager mgr;

    // Save a known state.
    mgr.loadString(R"({"build": {"parallelJobs": 4}})");
    CHECK(mgr.build().parallelJobs == 4);

    // Try loading invalid JSON.
    bool loaded = mgr.loadString("{ this is not valid json ");
    CHECK_FALSE(loaded);

    // Settings should be unchanged.
    CHECK(mgr.build().parallelJobs == 4);
}

TEST_CASE("Missing config files silently ignored", "[config][settings]") {
    SettingsManager mgr;

    // Point to directories that don't exist.
    mgr.setSystemConfigDir("/nonexistent/path/system");
    mgr.setUserConfigDir("/nonexistent/path/user");
    mgr.setProjectConfigDir("/nonexistent/path/project");

    // loadAll should not throw or crash, and settings should be defaults.
    int loaded = mgr.loadAll();
    // It will load 0 layers successfully since all paths are nonexistent.
    // But defaults should still be set by loadAll's resetToDefaults() call.
    CHECK(mgr.settings().general.language == "en");
}

// End of settings manager tests
