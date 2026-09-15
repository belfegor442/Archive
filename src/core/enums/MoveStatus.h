#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class MoveStatus {
    Planned,
    Executing,
    Completed,
    Failed,
    Skipped
};

inline std::string to_string(MoveStatus status) {
    switch (status) {
        case MoveStatus::Planned:    return "planned";
        case MoveStatus::Executing:  return "executing";
        case MoveStatus::Completed:  return "completed";
        case MoveStatus::Failed:     return "failed";
        case MoveStatus::Skipped:    return "skipped";
    }
    throw std::invalid_argument("Unknown MoveStatus");
}

inline MoveStatus move_status_from_string(const std::string& s) {
    if (s == "planned")    return MoveStatus::Planned;
    if (s == "executing")  return MoveStatus::Executing;
    if (s == "completed")  return MoveStatus::Completed;
    if (s == "failed")     return MoveStatus::Failed;
    if (s == "skipped")    return MoveStatus::Skipped;
    throw std::invalid_argument("Unknown MoveStatus: " + s);
}

} // namespace archive::core
