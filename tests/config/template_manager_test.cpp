// ==========================================================================
// TemplateManager tests
//
// Tests cover:
//   - Built-in template registration
//   - Template lookup by name
//   - Template listing
//   - Template rendering with {{placeholder}} substitution
//   - Template registration (add/override)
//   - Reset to defaults
// ==========================================================================

#include <catch2/catch_test_macros.hpp>

#include "lazycmake/config/template_manager.hpp"

using namespace lazycmake::config;

TEST_CASE("Built-in console_app template is available", "[config][template]") {
    TemplateManager mgr;

    const ProjectTemplate* tmpl = mgr.findTemplate("console_app");
    REQUIRE(tmpl != nullptr);
    CHECK(tmpl->name == "console_app");
    CHECK(tmpl->defaultTargetKind == "Executable");
    CHECK(tmpl->language == "Cpp");

    // Should have at least a src/main.cpp template file.
    CHECK_FALSE(tmpl->files.empty());
    bool hasMainCpp = false;
    for (const auto& f : tmpl->files) {
        if (f.sourcePath == "src/main.cpp") {
            hasMainCpp = true;
            // Should contain the {{project_name}} placeholder.
            CHECK(f.content.find("{{project_name}}") != std::string::npos);
            break;
        }
    }
    CHECK(hasMainCpp);
}

TEST_CASE("All built-in templates are listed", "[config][template]") {
    TemplateManager mgr;

    auto templates = mgr.availableTemplates();
    CHECK(templates.size() >= 4);

    bool hasConsole = false, hasStaticLib = false, hasHeaderOnly = false, hasCApp = false;
    for (const auto& name : templates) {
        if (name == "console_app") hasConsole = true;
        if (name == "static_library") hasStaticLib = true;
        if (name == "header_only_library") hasHeaderOnly = true;
        if (name == "c_console_app") hasCApp = true;
    }
    CHECK(hasConsole);
    CHECK(hasStaticLib);
    CHECK(hasHeaderOnly);
    CHECK(hasCApp);
}

TEST_CASE("Template rendering substitutes placeholders", "[config][template]") {
    TemplateManager mgr;

    TemplateContext ctx;
    ctx.projectName = "MyGame";
    ctx.projectNamespace = "my_game";
    ctx.authorName = "Alice";
    ctx.year = "2026";

    auto rendered = mgr.render("console_app", ctx);
    REQUIRE(rendered.has_value());

    // The result should have the template's files with substituted content.
    CHECK_FALSE(rendered->empty());

    // Find src/main.cpp and check substitution.
    auto it = rendered->find("src/main.cpp");
    REQUIRE(it != rendered->end());

    // The {{project_name}} should have been replaced with "MyGame".
    CHECK(it->second.find("MyGame") != std::string::npos);
    CHECK(it->second.find("{{project_name}}") == std::string::npos);
}

TEST_CASE("Static library template renders namespace correctly", "[config][template]") {
    TemplateManager mgr;

    TemplateContext ctx;
    ctx.projectName = "MathLib";
    ctx.projectNamespace = "math_lib";

    auto rendered = mgr.render("static_library", ctx);
    REQUIRE(rendered.has_value());

    // Check include/lib.hpp has namespace substitution.
    auto it = rendered->find("include/lib.hpp");
    REQUIRE(it != rendered->end());

    // The {{project_namespace}} should have been replaced.
    CHECK(it->second.find("math_lib") != std::string::npos);
    CHECK(it->second.find("{{project_namespace}}") == std::string::npos);
}

TEST_CASE("Rendering nonexistent template returns nullopt", "[config][template]") {
    TemplateManager mgr;

    TemplateContext ctx;
    ctx.projectName = "Test";

    auto rendered = mgr.render("nonexistent_template", ctx);
    CHECK_FALSE(rendered.has_value());
}

TEST_CASE("Registering a custom template works", "[config][template]") {
    TemplateManager mgr;

    ProjectTemplate custom;
    custom.name = "my_custom";
    custom.description = "My custom template";
    custom.defaultTargetKind = "Executable";
    custom.language = "Cpp";

    TemplateFile mainFile;
    mainFile.sourcePath = "src/main.cpp";
    mainFile.content = "// {{project_name}} main file\nint main() { return 0; }";
    custom.files.push_back(std::move(mainFile));

    mgr.registerTemplate(custom);

    // Should be findable.
    const ProjectTemplate* found = mgr.findTemplate("my_custom");
    REQUIRE(found != nullptr);
    CHECK(found->description == "My custom template");

    // Should appear in available templates.
    auto templates = mgr.availableTemplates();
    bool foundCustom = false;
    for (const auto& name : templates) {
        if (name == "my_custom") foundCustom = true;
    }
    CHECK(foundCustom);
}

TEST_CASE("Reset to defaults removes custom templates", "[config][template]") {
    TemplateManager mgr;

    // Register a custom template.
    ProjectTemplate custom;
    custom.name = "temporary";
    mgr.registerTemplate(custom);
    CHECK(mgr.findTemplate("temporary") != nullptr);

    // Reset.
    mgr.resetToDefaults();

    // Custom template should be gone.
    CHECK(mgr.findTemplate("temporary") == nullptr);

    // Built-in templates should still be there.
    CHECK(mgr.findTemplate("console_app") != nullptr);
}

TEST_CASE("Header-only library template is available", "[config][template]") {
    TemplateManager mgr;

    const ProjectTemplate* tmpl = mgr.findTemplate("header_only_library");
    REQUIRE(tmpl != nullptr);
    CHECK(tmpl->defaultTargetKind == "InterfaceLibrary");
    CHECK(tmpl->language == "Cpp");

    // Should have a template file with a Box template class.
    CHECK_FALSE(tmpl->files.empty());
}

TEST_CASE("C console app template has correct language", "[config][template]") {
    TemplateManager mgr;

    const ProjectTemplate* tmpl = mgr.findTemplate("c_console_app");
    REQUIRE(tmpl != nullptr);
    CHECK(tmpl->language == "C");
    CHECK(tmpl->defaultTargetKind == "Executable");

    bool hasMainC = false;
    for (const auto& f : tmpl->files) {
        if (f.sourcePath == "src/main.c") {
            hasMainC = true;
            break;
        }
    }
    CHECK(hasMainC);
}

// End of template manager tests
