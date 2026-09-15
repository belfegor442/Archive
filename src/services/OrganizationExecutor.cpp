#include "OrganizationExecutor.h"

#include <filesystem>
#include <algorithm>
#include <unordered_set>

#include "../core/utils/Uuid.h"
#include "../core/utils/Logger.h"
#include "../filesystem/FileUtils.h"
#include "../storage/Transaction.h"

namespace archive::services {

using namespace archive::core;

OrganizationExecutor::OrganizationExecutor(storage::DatabaseManager& db,
                                           storage::OrgPlanRepository& plans,
                                           storage::OrgMoveRepository& moves,
                                           storage::UndoRepository& undo)
    : db_(db)
    , plans_(plans)
    , moves_(moves)
    , undo_(undo)
{}

core::UndoRecord OrganizationExecutor::execute(const std::string& plan_id) {
    auto plan_opt = plans_.find_by_id(plan_id);
    if (!plan_opt) {
        throw std::runtime_error("Plan not found: " + plan_id);
    }

    core::OrgPlan plan = *plan_opt;
    if (plan.status != core::PlanStatus::Ready) {
        throw std::runtime_error("Plan is not approved for execution");
    }

    plan.status = core::PlanStatus::Executing;
    plans_.update(plan);

    auto plan_moves = moves_.find_by_plan(plan_id);

    core::UndoRecord undo_record;
    undo_record.id = core::utils::generate_id();
    undo_record.operation_id = plan_id;
    undo_record.moves_count = 0;
    undo_record.root_path = plan.root_path;
    undo_record.status = core::UndoStatus::Available;
    undo_record.created_at = core::utils::now_iso();

    std::vector<core::UndoEntry> undo_entries;
    undo_entries.reserve(plan_moves.size());
    int success_count = 0;
    int fail_count = 0;

    std::unordered_set<std::string> created_dirs;
    created_dirs.reserve(plan_moves.size());

    for (auto& move : plan_moves) {
        move.status = core::MoveStatus::Executing;
        moves_.update_status(move.id, core::MoveStatus::Executing);

        if (execute_move(move, created_dirs)) {
            core::UndoEntry entry;
            entry.undo_id = undo_record.id;
            entry.source_path = move.dest_path;
            entry.dest_path = move.source_path;
            entry.move_index = success_count;
            undo_entries.push_back(entry);

            move.status = core::MoveStatus::Completed;
            moves_.update_status(move.id, core::MoveStatus::Completed);
            success_count++;
        } else {
            move.status = core::MoveStatus::Failed;
            moves_.update_status(move.id, core::MoveStatus::Failed);
            fail_count++;
        }
    }

    undo_record.moves_count = success_count;

    {
        storage::Transaction tx(db_);
        undo_.insert_record(undo_record);
        if (!undo_entries.empty()) {
            undo_.insert_entries(undo_entries);
        }

        plan.status = fail_count > 0 ? core::PlanStatus::Failed : core::PlanStatus::Completed;
        plan.executed_at = core::utils::now_iso();
        plans_.update(plan);

        tx.commit();
    }

    return undo_record;
}

void OrganizationExecutor::undo(const std::string& undo_id) {
    auto record_opt = undo_.find_record_by_id(undo_id);
    if (!record_opt) {
        throw std::runtime_error("Undo record not found: " + undo_id);
    }

    core::UndoRecord record = *record_opt;
    if (record.status != core::UndoStatus::Available) {
        throw std::runtime_error("Undo record is not available");
    }

    auto entries = undo_.find_entries(undo_id);

    int success_count = 0;
    int fail_count = 0;

    for (const auto& entry : entries) {
        if (undo_move(entry)) {
            success_count++;
        } else {
            fail_count++;
        }
    }

    record.status = fail_count > 0 ? core::UndoStatus::Failed : core::UndoStatus::Used;
    undo_.update_status(undo_id, record.status);

    if (record.operation_id.find("plan_") == 0 ||
        record.operation_id.size() > 10) {
        auto plan_opt = plans_.find_by_id(record.operation_id);
        if (plan_opt) {
            core::OrgPlan plan = *plan_opt;
            plan.status = core::PlanStatus::Failed;
            plans_.update(plan);
        }
    }
}

bool OrganizationExecutor::can_write_to(const std::string& path) {
    namespace fs = std::filesystem;

    fs::path target(path);

    if (fs::exists(target)) {
        auto perms = fs::status(target).permissions();
        return (perms & fs::perms::owner_write) != fs::perms::none;
    }

    fs::path parent = target.parent_path();
    while (!parent.empty() && !fs::exists(parent)) {
        parent = parent.parent_path();
    }

    if (parent.empty()) return false;

    auto perms = fs::status(parent).permissions();
    return (perms & fs::perms::owner_write) != fs::perms::none;
}

bool OrganizationExecutor::execute_move(const core::OrgMove& move, std::unordered_set<std::string>& created_dirs) {
    try {
        namespace fs = std::filesystem;

        if (!fs::exists(move.source_path)) {
            LOG_WARN("Source file does not exist: " + move.source_path);
            return false;
        }

        ensure_dest_dir(move.dest_path, created_dirs);

        if (fs::exists(move.dest_path)) {
            std::string ext = filesystem::FileUtils::extension(move.dest_path);
            std::string file_stem = filesystem::FileUtils::stem(move.dest_path);
            std::string dir = filesystem::FileUtils::parent_dir(move.dest_path);

            int counter = 1;
            std::string new_dest = move.dest_path;
            while (fs::exists(new_dest)) {
                std::string new_name = file_stem + "_" + std::to_string(counter) + ext;
                new_dest = dir + "/" + new_name;
                counter++;
            }

            fs::rename(move.source_path, new_dest);
        } else {
            fs::rename(move.source_path, move.dest_path);
        }

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to move file: " + move.source_path + " -> " + move.dest_path
                  + " - " + e.what());
        return false;
    }
}

bool OrganizationExecutor::undo_move(const core::UndoEntry& entry) {
    try {
        namespace fs = std::filesystem;

        if (!fs::exists(entry.source_path)) {
            LOG_WARN("Undo source does not exist: " + entry.source_path);
            return false;
        }

        std::unordered_set<std::string> dummy;
        ensure_dest_dir(entry.dest_path, dummy);

        fs::rename(entry.source_path, entry.dest_path);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to undo move: " + entry.source_path + " -> " + entry.dest_path
                  + " - " + e.what());
        return false;
    }
}

void OrganizationExecutor::ensure_dest_dir(const std::string& dest_path, std::unordered_set<std::string>& created_dirs) {
    namespace fs = std::filesystem;

    fs::path dest(dest_path);
    fs::path parent = dest.parent_path();
    std::string parent_str = parent.string();

    if (created_dirs.count(parent_str)) return;

    if (!fs::exists(parent)) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) {
            throw std::runtime_error("Failed to create directory: " + parent.string()
                                     + " - " + ec.message());
        }
    }

    created_dirs.insert(parent_str);
}

} // namespace archive::services
