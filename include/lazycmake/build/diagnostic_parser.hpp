#pragma once

#include <string>
#include <vector>

namespace lazycmake::build {

enum class DiagnosticSeverity { Info, Warning, Error };

struct Diagnostic {
    std::string file;
    int line = 0;
    int column = 0;
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    std::string message;
    std::string rawLine;
};

class DiagnosticParser {
public:
    explicit DiagnosticParser(bool enableColor = false);

    std::vector<Diagnostic> parseLine(const std::string& line) const;
    bool isWarningOrError(const std::string& line) const;
    DiagnosticSeverity classify(const std::string& line) const;

private:
    bool enableColor_;
};

} // namespace lazycmake::build
