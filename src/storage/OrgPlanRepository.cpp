#include "OrgPlanRepository.h"
#include "../core/enums/PlanStatus.h"

namespace archive::storage {

OrgPlanRepository::OrgPlanRepository(DatabaseManager& db)
    : db_(db)
{}

void OrgPlanRepository::insert(const core::OrgPlan& plan) {
    auto stmt = db_.prepare(R"(
        INSERT INTO org_plans (id, scan_id, root_path, status, total_files,
            moves_planned, unchanged, avg_confidence, intensity, created_at, executed_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, plan.id);
    stmt.bind_text(2, plan.scan_id);
    stmt.bind_text(3, plan.root_path);
    stmt.bind_text(4, core::to_string(plan.status));
    stmt.bind_int(5, plan.total_files);
    stmt.bind_int(6, plan.moves_planned);
    stmt.bind_int(7, plan.unchanged);
    stmt.bind_int(8, static_cast<int>(plan.avg_confidence * 100));
    stmt.bind_int(9, plan.intensity);
    stmt.bind_text(10, plan.created_at);
    stmt.bind_text(11, plan.executed_at);
    stmt.step_done();
}

void OrgPlanRepository::update(const core::OrgPlan& plan) {
    auto stmt = db_.prepare(R"(
        UPDATE org_plans SET scan_id=?, root_path=?, status=?, total_files=?,
            moves_planned=?, unchanged=?, avg_confidence=?, intensity=?,
            created_at=?, executed_at=?
        WHERE id=?
    )");
    stmt.bind_text(1, plan.scan_id);
    stmt.bind_text(2, plan.root_path);
    stmt.bind_text(3, core::to_string(plan.status));
    stmt.bind_int(4, plan.total_files);
    stmt.bind_int(5, plan.moves_planned);
    stmt.bind_int(6, plan.unchanged);
    stmt.bind_int(7, static_cast<int>(plan.avg_confidence * 100));
    stmt.bind_int(8, plan.intensity);
    stmt.bind_text(9, plan.created_at);
    stmt.bind_text(10, plan.executed_at);
    stmt.bind_text(11, plan.id);
    stmt.step_done();
}

std::optional<core::OrgPlan> OrgPlanRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, scan_id, root_path, status, total_files, moves_planned,
            unchanged, avg_confidence, intensity, created_at, executed_at
        FROM org_plans WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_plan(stmt);
    return std::nullopt;
}

std::vector<core::OrgPlan> OrgPlanRepository::find_all() {
    std::vector<core::OrgPlan> plans;
    auto stmt = db_.prepare(R"(
        SELECT id, scan_id, root_path, status, total_files, moves_planned,
            unchanged, avg_confidence, intensity, created_at, executed_at
        FROM org_plans ORDER BY created_at DESC
    )");
    while (stmt.step()) {
        plans.push_back(read_plan(stmt));
    }
    return plans;
}

std::optional<core::OrgPlan> OrgPlanRepository::find_latest_by_root(const std::string& root_path) {
    auto stmt = db_.prepare(R"(
        SELECT id, scan_id, root_path, status, total_files, moves_planned,
            unchanged, avg_confidence, intensity, created_at, executed_at
        FROM org_plans WHERE root_path=? ORDER BY created_at DESC LIMIT 1
    )");
    stmt.bind_text(1, root_path);
    if (stmt.step()) return read_plan(stmt);
    return std::nullopt;
}

core::OrgPlan OrgPlanRepository::read_plan(DatabaseManager::Statement& stmt) {
    core::OrgPlan plan;
    plan.id = stmt.column_text(0);
    plan.scan_id = stmt.column_text(1);
    plan.root_path = stmt.column_text(2);
    plan.status = core::plan_status_from_string(stmt.column_text(3));
    plan.total_files = stmt.column_int(4);
    plan.moves_planned = stmt.column_int(5);
    plan.unchanged = stmt.column_int(6);
    plan.avg_confidence = static_cast<double>(stmt.column_int(7)) / 100.0;
    plan.intensity = stmt.column_int(8);
    plan.created_at = stmt.column_text(9);
    plan.executed_at = stmt.column_text(10);
    return plan;
}

} // namespace archive::storage
