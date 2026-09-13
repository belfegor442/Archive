#include "Diagnostics.hpp"

#include <sstream>

namespace monix::renderer_vk {

void Diagnostics::info(std::string message) {
    add(Diagnostic{DiagnosticSeverity::Info, {}, 0, std::move(message)});
}

void Diagnostics::warning(std::string message) {
    add(Diagnostic{DiagnosticSeverity::Warning, {}, 0, std::move(message)});
}

void Diagnostics::error(std::string message) {
    add(Diagnostic{DiagnosticSeverity::Error, {}, 0, std::move(message)});
}

void Diagnostics::add(Diagnostic diagnostic) {
    entries_.push_back(std::move(diagnostic));
}

bool Diagnostics::hasErrors() const {
    for (const auto& entry : entries_) {
        if (entry.severity == DiagnosticSeverity::Error) {
            return true;
        }
    }
    return false;
}

std::string Diagnostics::summary() const {
    std::ostringstream out;
    for (const auto& entry : entries_) {
        switch (entry.severity) {
        case DiagnosticSeverity::Info: out << "info"; break;
        case DiagnosticSeverity::Warning: out << "warning"; break;
        case DiagnosticSeverity::Error: out << "error"; break;
        }
        if (!entry.file.empty()) {
            out << " " << entry.file.string();
            if (entry.line > 0) {
                out << ":" << entry.line;
            }
        }
        out << ": " << entry.message << "\n";
    }
    return out.str();
}

}  // namespace monix::renderer_vk
