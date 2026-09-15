#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class ScanStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

inline std::string to_string(ScanStatus status) {
    switch (status) {
        case ScanStatus::Pending:   return "pending";
        case ScanStatus::Running:   return "running";
        case ScanStatus::Completed: return "completed";
        case ScanStatus::Failed:    return "failed";
        case ScanStatus::Cancelled: return "cancelled";
    }
    throw std::invalid_argument("Unknown ScanStatus");
}

inline ScanStatus scan_status_from_string(const std::string& s) {
    if (s == "pending")   return ScanStatus::Pending;
    if (s == "running")   return ScanStatus::Running;
    if (s == "completed") return ScanStatus::Completed;
    if (s == "failed")    return ScanStatus::Failed;
    if (s == "cancelled") return ScanStatus::Cancelled;
    throw std::invalid_argument("Unknown ScanStatus: " + s);
}

} // namespace archive::core
