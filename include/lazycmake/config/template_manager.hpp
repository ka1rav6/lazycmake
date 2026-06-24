#pragma once

// ==========================================================================
// TemplateManager — discovers and manages project templates (§15.4)
//
// A project template is a directory with a template.json metadata file and
// source files using {{placeholder}} syntax for substitution.
//
// Templates are discovered from:
//   1. Built-in templates (resources/templates/) compiled into the binary.
//   2. User templates (~/.config/lazycmake/templates/<name>/)
//   3. Plugin-contributed templates (via PluginHost, Phase 7)
//
// User templates override built-in templates with the same name,
// following the same layered resolution pattern as other config surfaces.
//
// The New Project Wizard (Phase 5/6) uses this manager to populate its
// template selection step.
// ==========================================================================

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "lazycmake/config/config_types.hpp"

namespace lazycmake::config {

// Information about the substitution context used when rendering
// a template. These values are provided by the New Project Wizard.
struct TemplateContext {
    std::string projectName;        // e.g. "MyGame"
    std::string projectNamespace;   // e.g. "my_game" (sanitized name)
    std::string authorName;         // e.g. "Alice"
    std::string year;               // e.g. "2026"
};

class TemplateManager {
public:
    TemplateManager();

    // Discover all templates from built-in sources and user directories.
    // Returns the number of templates found.
    int loadAll();

    // Discover templates from a specific directory.
    // Each subdirectory containing a template.json is loaded as a template.
    // Returns the number loaded from this directory.
    int discoverFrom(const std::filesystem::path& directory);

    // Get a template by name. Returns nullptr if not found.
    [[nodiscard]] const ProjectTemplate* findTemplate(const std::string& name) const;

    // List all available template names.
    [[nodiscard]] std::vector<std::string> availableTemplates() const;

    // Render a template: substitute {{placeholders}} with values from
    // the given context. Returns the rendered files as a map of relative
    // path -> content, ready to be written to the new project directory.
    //
    // Returns std::nullopt if the template is not found.
    [[nodiscard]] std::optional<std::map<std::filesystem::path, std::string>>
    render(const std::string& templateName, const TemplateContext& context) const;

    // Register a template (useful for tests and plugin-contributed templates).
    void registerTemplate(ProjectTemplate tmpl);

    // Reset to built-in defaults.
    void resetToDefaults();

    // Set user templates directory.
    void setUserTemplatesDir(const std::filesystem::path& path);

private:
    std::map<std::string, ProjectTemplate> templates_;
    std::filesystem::path userTemplatesDir_;

    // Create the built-in default templates.
    void registerBuiltinTemplates();

    // Apply {{placeholder}} substitution to a template string.
    [[nodiscard]] std::string substitute(const std::string& template_content,
                                          const TemplateContext& context) const;
};

} // namespace lazycmake::config
