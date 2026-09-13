#pragma once

#include "../../core/Diagnostics.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace monix::renderer_vk {

enum class ShaderDiagnosticKind : std::uint8_t {
    None = 0,
    LoadError,
    PreprocessError,
    IncludeError,
    CompileError,
    ReflectionError,
    PipelineError,
    UnsupportedLanguage,
    ValidationError
};

inline const char* shaderDiagnosticKindName(ShaderDiagnosticKind kind) {
    switch (kind) {
    case ShaderDiagnosticKind::None:                return "NONE";
    case ShaderDiagnosticKind::LoadError:           return "LOAD_ERROR";
    case ShaderDiagnosticKind::PreprocessError:     return "PREPROCESS_ERROR";
    case ShaderDiagnosticKind::IncludeError:        return "INCLUDE_ERROR";
    case ShaderDiagnosticKind::CompileError:        return "COMPILE_ERROR";
    case ShaderDiagnosticKind::ReflectionError:     return "REFLECTION_ERROR";
    case ShaderDiagnosticKind::PipelineError:       return "PIPELINE_ERROR";
    case ShaderDiagnosticKind::UnsupportedLanguage: return "UNSUPPORTED_LANGUAGE";
    case ShaderDiagnosticKind::ValidationError:     return "VALIDATION_ERROR";
    }
    return "UNKNOWN";
}

struct ShaderDiagnostic {
    ShaderDiagnosticKind kind = ShaderDiagnosticKind::None;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    std::string file;
    int line = 0;
    int column = 0;
    std::string stage;
    std::string errorCode;
    std::string message;
};

class ShaderDiagnostics {
public:
    void add(ShaderDiagnostic diag);
    bool hasErrors() const;
    bool hasWarnings() const;
    std::string summary() const;
    std::string formatAll() const;
    const std::vector<ShaderDiagnostic>& entries() const { return entries_; }

    void clear();

    static ShaderDiagnostics makeError(ShaderDiagnosticKind kind, std::string message);
    static ShaderDiagnostics makeError(ShaderDiagnosticKind kind, std::string file, int line, std::string message);
    static ShaderDiagnostics makeWarning(std::string message);
    static ShaderDiagnostics makeUnsupported(const std::string& language, const std::string& reason);

private:
    std::vector<ShaderDiagnostic> entries_;
};

}  // namespace monix::renderer_vk
