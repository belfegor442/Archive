#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class PlanStatus {
    Draft,
    Ready,
    Executing,
    Completed,
    Failed
};

inline std::string to_string(PlanStatus status) {
    switch (status) {
        case PlanStatus::Draft:     return "draft";
        case PlanStatus::Ready:     return "ready";
        case PlanStatus::Executing: return "executing";
        case PlanStatus::Completed: return "completed";
        case PlanStatus::Failed:    return "failed";
    }
    throw std::invalid_argument("Unknown PlanStatus");
}

inline PlanStatus plan_status_from_string(const std::string& s) {
    if (s == "draft")     return PlanStatus::Draft;
    if (s == "ready")     return PlanStatus::Ready;
    if (s == "executing") return PlanStatus::Executing;
    if (s == "completed") return PlanStatus::Completed;
    if (s == "failed")    return PlanStatus::Failed;
    throw std::invalid_argument("Unknown PlanStatus: " + s);
}

} // namespace archive::core
