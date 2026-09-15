#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/OrgPlan.h"
#include "DatabaseManager.h"

namespace archive::storage {

class OrgPlanRepository {
public:
    explicit OrgPlanRepository(DatabaseManager& db);

    void insert(const core::OrgPlan& plan);
    void update(const core::OrgPlan& plan);
    std::optional<core::OrgPlan> find_by_id(const std::string& id);
    std::vector<core::OrgPlan> find_all();
    std::optional<core::OrgPlan> find_latest_by_root(const std::string& root_path);

private:
    DatabaseManager& db_;
    core::OrgPlan read_plan(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
