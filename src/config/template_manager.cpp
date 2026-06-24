#include "lazycmake/config/template_manager.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace lazycmake::config {

TemplateManager::TemplateManager() {
    const char* home = std::getenv("HOME");
    if (home) {
        userTemplatesDir_ = std::filesystem::path(home) / ".config" / "lazycmake" / "templates";
    }

    // Always register built-in templates.
    registerBuiltinTemplates();
}

int TemplateManager::loadAll() {
    int count = 0;

    // Discover user templates.
    if (!userTemplatesDir_.empty() && std::filesystem::exists(userTemplatesDir_)) {
        count += discoverFrom(userTemplatesDir_);
    }

    return count;
}

int TemplateManager::discoverFrom(const std::filesystem::path& directory) {
    int count = 0;
    std::error_code ec;

    if (!std::filesystem::exists(directory, ec)) return 0;

    for (const auto& entry : std::filesystem::directory_iterator(directory, ec)) {
        if (!entry.is_directory()) continue;

        std::filesystem::path metadataPath = entry.path() / "template.json";
        if (!std::filesystem::exists(metadataPath, ec)) continue;

        std::ifstream file(metadataPath);
        if (!file.is_open()) continue;

        try {
            nlohmann::json j;
            file >> j;
            ProjectTemplate tmpl = j.get<ProjectTemplate>();

            // Also load template files from the directory.
            // For each file in the template directory (except template.json
            // itself), read it as a TemplateFile with {{placeholder}} syntax.
            for (const auto& tmplEntry : std::filesystem::recursive_directory_iterator(
                     entry.path(), ec)) {
                if (tmplEntry.is_directory()) continue;

                std::filesystem::path relativePath = std::filesystem::relative(
                    tmplEntry.path(), entry.path(), ec);

                // Skip the metadata file itself.
                if (relativePath == "template.json") continue;

                std::ifstream tmplFile(tmplEntry.path());
                if (!tmplFile.is_open()) continue;

                std::stringstream buffer;
                buffer << tmplFile.rdbuf();

                tmpl.files.push_back({
                    .sourcePath = relativePath,
                    .content = buffer.str(),
                });
            }

            registerTemplate(std::move(tmpl));
            ++count;
        } catch (const std::exception& e) {
            std::cerr << "TemplateManager: Error loading template from "
                      << entry.path() << ": " << e.what() << std::endl;
        }
    }

    return count;
}

const ProjectTemplate* TemplateManager::findTemplate(const std::string& name) const {
    auto it = templates_.find(name);
    if (it != templates_.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> TemplateManager::availableTemplates() const {
    std::vector<std::string> names;
    names.reserve(templates_.size());
    for (const auto& [name, _] : templates_) {
        names.push_back(name);
    }
    return names;
}

std::optional<std::map<std::filesystem::path, std::string>>
TemplateManager::render(const std::string& templateName,
                         const TemplateContext& context) const {
    const ProjectTemplate* tmpl = findTemplate(templateName);
    if (!tmpl) return std::nullopt;

    std::map<std::filesystem::path, std::string> result;

    for (const auto& file : tmpl->files) {
        std::string rendered = substitute(file.content, context);
        result[file.sourcePath] = std::move(rendered);
    }

    return result;
}

void TemplateManager::registerTemplate(ProjectTemplate tmpl) {
    // User/plugin templates override built-in ones with the same name
    // (layered resolution pattern).
    templates_[tmpl.name] = std::move(tmpl);
}

void TemplateManager::resetToDefaults() {
    templates_.clear();
    registerBuiltinTemplates();
}

void TemplateManager::setUserTemplatesDir(const std::filesystem::path& path) {
    userTemplatesDir_ = path;
}

void TemplateManager::registerBuiltinTemplates() {
    // ---- Console executable template ----
    ProjectTemplate consoleApp;
    consoleApp.name = "console_app";
    consoleApp.description = "Simple C++ console application";
    consoleApp.defaultTargetKind = "Executable";
    consoleApp.defaultSourceFiles = {"src/main.cpp"};
    consoleApp.language = "Cpp";

    // A simple main.cpp template.
    TemplateFile mainCpp;
    mainCpp.sourcePath = "src/main.cpp";
    mainCpp.content = R"(#include <iostream>

int main() {
    std::cout << "Hello from {{project_name}}!" << std::endl;
    return 0;
}
)";
    consoleApp.files.push_back(std::move(mainCpp));

    // CMakeLists.txt template (a stub — the generator will overwrite this).
    TemplateFile cmakeLists;
    cmakeLists.sourcePath = "CMakeLists.txt";
    cmakeLists.content = R"(# CMakeLists.txt for {{project_name}}
# This file will be regenerated by LazyCMake on first configure.
cmake_minimum_required(VERSION 3.21)
project({{project_name}} LANGUAGES CXX)

add_executable(${PROJECT_NAME} src/main.cpp)
)";
    consoleApp.files.push_back(std::move(cmakeLists));

    registerTemplate(std::move(consoleApp));

    // ---- Static library template ----
    ProjectTemplate staticLib;
    staticLib.name = "static_library";
    staticLib.description = "Static library with header and source";
    staticLib.defaultTargetKind = "StaticLibrary";
    staticLib.defaultSourceFiles = {"src/lib.cpp", "include/lib.hpp"};
    staticLib.language = "Cpp";

    TemplateFile libHpp;
    libHpp.sourcePath = "include/lib.hpp";
    libHpp.content = R"(#pragma once

namespace {{project_namespace}} {

int add(int a, int b);

} // namespace {{project_namespace}}
)";
    staticLib.files.push_back(std::move(libHpp));

    TemplateFile libCpp;
    libCpp.sourcePath = "src/lib.cpp";
    libCpp.content = R"(#include "{{project_namespace}}/lib.hpp"

namespace {{project_namespace}} {

int add(int a, int b) {
    return a + b;
}

} // namespace {{project_namespace}}
)";
    staticLib.files.push_back(std::move(libCpp));

    registerTemplate(std::move(staticLib));

    // ---- Header-only library template ----
    ProjectTemplate headerOnly;
    headerOnly.name = "header_only_library";
    headerOnly.description = "Header-only (interface) library";
    headerOnly.defaultTargetKind = "InterfaceLibrary";
    headerOnly.defaultSourceFiles = {"include/lib.hpp"};
    headerOnly.language = "Cpp";

    TemplateFile header;
    header.sourcePath = "include/lib.hpp";
    header.content = R"(#pragma once

namespace {{project_namespace}} {

template <typename T>
class Box {
public:
    explicit Box(T value) : value_(value) {}
    T get() const { return value_; }
    void set(T value) { value_ = value; }

private:
    T value_;
};

} // namespace {{project_namespace}}
)";
    headerOnly.files.push_back(std::move(header));

    registerTemplate(std::move(headerOnly));

    // ---- C executable template ----
    ProjectTemplate cApp;
    cApp.name = "c_console_app";
    cApp.description = "Simple C console application";
    cApp.defaultTargetKind = "Executable";
    cApp.defaultSourceFiles = {"src/main.c"};
    cApp.language = "C";

    TemplateFile mainC;
    mainC.sourcePath = "src/main.c";
    mainC.content = R"(#include <stdio.h>

int main() {
    printf("Hello from {{project_name}}!\n");
    return 0;
}
)";
    cApp.files.push_back(std::move(mainC));

    registerTemplate(std::move(cApp));
}

std::string TemplateManager::substitute(const std::string& template_content,
                                          const TemplateContext& context) const {
    // Simple {{placeholder}} substitution using regex.
    // Supported placeholders:
    //   {{project_name}}       → "MyGame"
    //   {{project_namespace}}  → "my_game"
    //   {{author_name}}        → "Alice"
    //   {{year}}               → "2026"
    //
    // This is a basic implementation. A future enhancement could support
    // Mustache-style sections ({{#if}}...{{/if}}).

    std::string result = template_content;

    auto replace = [&](const std::string& placeholder, const std::string& value) {
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    };

    replace("{{project_name}}", context.projectName);
    replace("{{project_namespace}}", context.projectNamespace);
    replace("{{author_name}}", context.authorName);
    replace("{{year}}", context.year);

    return result;
}

} // namespace lazycmake::config
