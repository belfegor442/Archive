#include "ShaderDiagnostics.hpp"

#include <sstream>

namespace monix::renderer_vk {

void ShaderDiagnostics::add(ShaderDiagnostic diag) {
    entries_.push_back(std::move(diag));
}

bool ShaderDiagnostics::hasErrors() const {
    for (const auto& e : entries_) {
        if (e.severity == DiagnosticSeverity::Error) return true;
    }
    return false;
}

bool ShaderDiagnostics::hasWarnings() const {
    for (const auto& e : entries_) {
        if (e.severity == DiagnosticSeverity::Warning) return true;
    }
    return false;
}

std::string ShaderDiagnostics::summary() const {
    if (entries_.empty()) return "No diagnostics";
    std::ostringstream oss;
    int errors = 0, warnings = 0;
    for (const auto& e : entries_) {
        if (e.severity == DiagnosticSeverity::Error) ++errors;
        else if (e.severity == DiagnosticSeverity::Warning) ++warnings;
    }
    oss << errors << " error(s), " << warnings << " warning(s)";
    return oss.str();
}

std::string ShaderDiagnostics::formatAll() const {
    std::ostringstream oss;
    for (const auto& e : entries_) {
        const char* sev = (e.severity == DiagnosticSeverity::Error) ? "ERROR" :
                          (e.severity == DiagnosticSeverity::Warning) ? "WARNING" : "INFO";
        oss << "[" << sev << "] [" << shaderDiagnosticKindName(e.kind) << "]";
        if (!e.file.empty()) oss << " " << e.file;
        if (e.line > 0) oss << ":" << e.line;
        if (e.column > 0) oss << ":" << e.column;
        if (!e.stage.empty()) oss << " (" << e.stage << ")";
        if (!e.errorCode.empty()) oss << " [" << e.errorCode << "]";
        oss << " " << e.message << "\n";
    }
    return oss.str();
}

void ShaderDiagnostics::clear() {
    entries_.clear();
}

ShaderDiagnostics ShaderDiagnostics::makeError(ShaderDiagnosticKind kind, std::string message) {
    ShaderDiagnostics diags;
    diags.add(ShaderDiagnostic{kind, DiagnosticSeverity::Error, "", 0, 0, "", "", std::move(message)});
    return diags;
}

ShaderDiagnostics ShaderDiagnostics::makeError(ShaderDiagnosticKind kind, std::string file, int line, std::string message) {
    ShaderDiagnostics diags;
    diags.add(ShaderDiagnostic{kind, DiagnosticSeverity::Error, std::move(file), line, 0, "", "", std::move(message)});
    return diags;
}

ShaderDiagnostics ShaderDiagnostics::makeWarning(std::string message) {
    ShaderDiagnostics diags;
    diags.add(ShaderDiagnostic{ShaderDiagnosticKind::None, DiagnosticSeverity::Warning, "", 0, 0, "", "", std::move(message)});
    return diags;
}

ShaderDiagnostics ShaderDiagnostics::makeUnsupported(const std::string& language, const std::string& reason) {
    ShaderDiagnostics diags;
    diags.add(ShaderDiagnostic{
        ShaderDiagnosticKind::UnsupportedLanguage,
        DiagnosticSeverity::Error,
        "", 0, 0, "", "",
        language + " compiler unavailable. " + reason + ". The shader was not compiled and was not activated."
    });
    return diags;
}

}  // namespace monix::renderer_vk
