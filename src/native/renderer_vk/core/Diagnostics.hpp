#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace monix::renderer_vk {

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct Diagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Info;
    std::filesystem::path file;
    int line = 0;
    std::string message;
};

class Diagnostics {
public:
    void info(std::string message);
    void warning(std::string message);
    void error(std::string message);
    void add(Diagnostic diagnostic);

    bool hasErrors() const;
    std::string summary() const;
    const std::vector<Diagnostic>& entries() const { return entries_; }

private:
    std::vector<Diagnostic> entries_;
};

}  // namespace monix::renderer_vk
