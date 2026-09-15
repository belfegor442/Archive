#pragma once

#include <string>
#include "../enums/PlanStatus.h"

namespace archive::core {

struct OrgPlan {
    std::string id;
    std::string scan_id;
    std::string root_path;
    PlanStatus status = PlanStatus::Draft;
    int total_files = 0;
    int moves_planned = 0;
    int unchanged = 0;
    double avg_confidence = 0.0;
    int intensity = 50;
    std::string created_at;
    std::string executed_at;

    OrgPlan() = default;
};

} // namespace archive::core
