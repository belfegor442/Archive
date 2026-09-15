#pragma once

#include <string>
#include <unordered_set>
#include "../core/models/OrgPlan.h"
#include "../core/models/OrgMove.h"
#include "../core/models/UndoRecord.h"
#include "../core/models/UndoEntry.h"
#include "../storage/DatabaseManager.h"
#include "../storage/OrgPlanRepository.h"
#include "../storage/OrgMoveRepository.h"
#include "../storage/UndoRepository.h"

namespace archive::services {

class OrganizationExecutor {
public:
    OrganizationExecutor(storage::DatabaseManager& db,
                         storage::OrgPlanRepository& plans,
                         storage::OrgMoveRepository& moves,
                         storage::UndoRepository& undo);

    core::UndoRecord execute(const std::string& plan_id);
    void undo(const std::string& undo_id);
    static bool can_write_to(const std::string& path);

private:
    storage::DatabaseManager& db_;
    storage::OrgPlanRepository& plans_;
    storage::OrgMoveRepository& moves_;
    storage::UndoRepository& undo_;

    bool execute_move(const core::OrgMove& move, std::unordered_set<std::string>& created_dirs);
    bool undo_move(const core::UndoEntry& entry);
    void ensure_dest_dir(const std::string& dest_path, std::unordered_set<std::string>& created_dirs);
};

} // namespace archive::services
