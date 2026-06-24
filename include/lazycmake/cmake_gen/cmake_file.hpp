#pragma once

// ==========================================================================
// CMakeFile / CMakeStatement — a small CMake AST (§10.2)
//
// The generator does NOT build CMakeLists.txt via string concatenation
// against the Project struct. Instead it first "lowers" a Project into a
// small typed AST, then a separate CMakePrinter renders that AST to text.
//
// Why this split (AST ≠ rendering)?
//   - A plugin can post-process the AST (e.g. inject a sanitizer's
//     target_compile_options) without string-patching generated text.
//   - Testing the AST builder and the printer independently is cleaner.
//   - The AST enforces the "never generate legacy CMake" rule structurally:
//     there is no node type for directory-scoped include_directories() etc.
//
// Each CMakeStatement holds a command name + its arguments. This simple
// "S-expression" style covers the entire modern CMake surface we need:
//   cmake_minimum_required, project, add_subdirectory,
//   add_library/add_executable, target_include_directories,
//   target_link_libraries, target_compile_features,
//   target_compile_definitions, include(), find_package,
//   FetchContent_Declare/FetchContent_MakeAvailable,
//   set() with cache/internal options.
// ==========================================================================

#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace lazycmake::cmake_gen {

// A single CMake command statement.
//
// Examples:
//   Command:       add_subdirectory
//   Arguments:     ["engine"]
//   Rendered:      add_subdirectory(engine)
//
//   Command:       target_link_libraries
//   Arguments:     ["app", "PUBLIC", "fmt::fmt", "engine"]
//   Rendered:      target_link_libraries(app PUBLIC fmt::fmt engine)
//
//   Command:       set
//   Arguments:     ["CMAKE_CXX_STANDARD", "20", "CACHE", "STRING", "..."],
//   Options:       {.quoteArgs = true}
//   Rendered:      set(CMAKE_CXX_STANDARD "20" CACHE STRING "...")
struct CMakeStatement {
    // The CMake command name (e.g. "add_library", "target_link_libraries").
    std::string command;

    // The arguments to the command, in order. Each is rendered as a single
    // token, optionally quoted (see quoteArgs). For most commands, CMake
    // doesn't require quoting of simple identifiers/paths, but paths
    // containing spaces or generator expressions in certain contexts do.
    std::vector<std::string> arguments;

    // If true, every argument is rendered inside double-quotes.
    // This is needed for set(CACHE ...) values, file paths with spaces,
    // and description strings.
    bool quoteArgs = false;

    // If true, don't add a trailing newline after this statement
    // (used for last statements in a block or for inline comments).
    bool noTrailingNewline = false;

    // Optional leading comment, rendered as "# comment" on the line before
    // the statement. This is how the generator marks generated-code
    // boundaries (e.g. "# LAZYCMAKE:BEGIN dependencies").
    std::string comment;

    // Convenience constructors.
    CMakeStatement() = default;

    explicit CMakeStatement(std::string cmd, std::vector<std::string> args = {},
                            bool quoted = false)
        : command(std::move(cmd))
        , arguments(std::move(args))
        , quoteArgs(quoted) {}

    CMakeStatement(std::string cmd, std::vector<std::string> args,
                   std::string cmt, bool quoted = false)
        : command(std::move(cmd))
        , arguments(std::move(args))
        , quoteArgs(quoted)
        , comment(std::move(cmt)) {}
};

// A complete CMake file, consisting of an ordered sequence of statements.
// This is the top-level AST node.
//
// The printer renders statements in order, separated by blank lines for
// readability (grouping logical blocks like "dependencies", "targets").
class CMakeFile {
public:
    // Add a statement to the end of this file.
    void addStatement(CMakeStatement stmt);

    // Add a raw comment line (rendered as "# text").
    void addComment(const std::string& text);

    // Add a blank line separator (rendered as an empty line).
    void addBlankLine();

    // Access all elements in order. Each element is either a statement,
    // a comment, or a blank line separator.
    enum class ElementType { Statement, Comment, BlankLine };

    struct Element {
        ElementType type;
        std::string comment;        // populated if type == Comment
        CMakeStatement statement;   // populated if type == Statement
    };

    [[nodiscard]] const std::vector<Element>& elements() const;

    // Number of elements.
    [[nodiscard]] std::size_t size() const;

    // Check if the file is empty.
    [[nodiscard]] bool empty() const;

    // Clear all elements.
    void clear();

private:
    std::vector<Element> elements_;
};

} // namespace lazycmake::cmake_gen
