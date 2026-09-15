#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/OrgMove.h"
#include "DatabaseManager.h"

namespace archive::storage {

class OrgMoveRepository {
public:
    explicit OrgMoveRepository(DatabaseManager& db);

    void insert(const core::OrgMove& move);
    void insert_batch(const std::vector<core::OrgMove>& moves);
    void update_status(const std::string& id, core::MoveStatus status);
    std::vector<core::OrgMove> find_by_plan(const std::string& plan_id);
    std::vector<core::OrgMove> find_by_plan_and_status(const std::string& plan_id, core::MoveStatus status);
    std::optional<core::OrgMove> find_by_id(const std::string& id);
    int count_by_plan(const std::string& plan_id);
    int count_by_plan_and_status(const std::string& plan_id, core::MoveStatus status);

private:
    DatabaseManager& db_;
    core::OrgMove read_move(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
