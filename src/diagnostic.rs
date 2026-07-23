// TODO: not yet wired — this parser is built but never called from the TUI.
// Intended usage: wire into BuildManager's onOutput callback to parse
// compiler diagnostics line-by-line.

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DiagnosticSeverity {
    Info,
    Warning,
    Error,
}

#[derive(Debug, Clone)]
pub struct Diagnostic {
    pub file: String,
    pub line: i32,
    pub column: i32,
    pub severity: DiagnosticSeverity,
    pub message: String,
    pub raw_line: String,
}

pub struct DiagnosticParser;

impl DiagnosticParser {
    pub fn new() -> Self {
        Self
    }

    pub fn is_warning_or_error(&self, line: &str) -> bool {
        let lower = line.to_lowercase();
        lower.contains("error:") || lower.contains("warning:")
    }

    pub fn classify(&self, line: &str) -> DiagnosticSeverity {
        let lower = line.to_lowercase();
        if lower.contains("error:") {
            DiagnosticSeverity::Error
        } else if lower.contains("warning:") {
            DiagnosticSeverity::Warning
        } else {
            DiagnosticSeverity::Info
        }
    }

    pub fn parse_line(&self, line: &str) -> Vec<Diagnostic> {
        let severity = self.classify(line);
        if severity == DiagnosticSeverity::Info {
            return Vec::new();
        }
        let mut diag = Diagnostic {
            file: String::new(),
            line: 0,
            column: 0,
            severity,
            message: line.to_string(),
            raw_line: line.to_string(),
        };
        // Simple GCC/Clang format parse: file:line:col: severity: message
        let parts: Vec<&str> = line.splitn(5, ':').collect();
        if parts.len() >= 5 {
            let sev = parts[3].trim().to_lowercase();
            if sev.contains("error") || sev.contains("warning") {
                diag.file = parts[0].trim().to_string();
                diag.line = parts[1].trim().parse().unwrap_or(0);
                diag.column = parts[2].trim().parse().unwrap_or(0);
                diag.message = parts[4].trim().to_string();
            }
        }
        vec![diag]
    }
}
