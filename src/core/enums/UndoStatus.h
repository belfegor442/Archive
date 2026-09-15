#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class UndoStatus {
    Available,
    Used,
    Failed
};

inline std::string to_string(UndoStatus status) {
    switch (status) {
        case UndoStatus::Available: return "available";
        case UndoStatus::Used:      return "used";
        case UndoStatus::Failed:    return "failed";
    }
    throw std::invalid_argument("Unknown UndoStatus");
}

inline UndoStatus undo_status_from_string(const std::string& s) {
    if (s == "available") return UndoStatus::Available;
    if (s == "used")      return UndoStatus::Used;
    if (s == "failed")    return UndoStatus::Failed;
    throw std::invalid_argument("Unknown UndoStatus: " + s);
}

} // namespace archive::core
