#include "lazycmake/build/diagnostic_parser.hpp"

#include <regex>

namespace lazycmake::build {

DiagnosticParser::DiagnosticParser(bool enableColor)
    : enableColor_(enableColor) {}

bool DiagnosticParser::isWarningOrError(const std::string& line) const {
    return classify(line) != DiagnosticSeverity::Info;
}

DiagnosticSeverity DiagnosticParser::classify(const std::string& line) const {
    static const std::regex errorRe(R"((error|fatal error)\s*:)",
                                     std::regex::icase);
    static const std::regex warningRe(R"((warning)\s*:)",
                                       std::regex::icase);

    if (std::regex_search(line, errorRe)) return DiagnosticSeverity::Error;
    if (std::regex_search(line, warningRe)) return DiagnosticSeverity::Warning;
    return DiagnosticSeverity::Info;
}

std::vector<Diagnostic> DiagnosticParser::parseLine(const std::string& line) const {
    std::vector<Diagnostic> result;
    auto severity = classify(line);
    if (severity == DiagnosticSeverity::Info) return result;

    Diagnostic diag;
    diag.rawLine = line;
    diag.severity = severity;
    diag.message = line;

    static const std::regex gccRe(
        R"(^(?:.*:)?\s*([^:]+):(\d+):(\d+):\s*(warning|error|fatal error):\s*(.*)$)",
        std::regex::icase);

    std::smatch match;
    if (std::regex_match(line, match, gccRe)) {
        diag.file = match[1].str();
        diag.line = std::stoi(match[2].str());
        diag.column = std::stoi(match[3].str());
        diag.message = match[5].str();
    }

    result.push_back(diag);
    return result;
}

} // namespace lazycmake::build
