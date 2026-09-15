#pragma once

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace archive::core {

struct OrgPlanSummary {
    std::string plan_id;
    int total_files = 0;
    int moves_planned = 0;
    int unchanged = 0;
    double avg_confidence = 0.0;
    std::map<std::string, int> by_category;
    std::vector<std::pair<std::string, double>> confidence_distribution;
};

} // namespace archive::core
