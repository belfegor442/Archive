#include "OrgMoveRepository.h"
#include "../core/enums/MoveStatus.h"

namespace archive::storage {

OrgMoveRepository::OrgMoveRepository(DatabaseManager& db)
    : db_(db)
{}

void OrgMoveRepository::insert(const core::OrgMove& move) {
    auto stmt = db_.prepare(R"(
        INSERT INTO org_moves (id, plan_id, scan_item_id, source_path, dest_path,
            confidence, reason, status)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, move.id);
    stmt.bind_text(2, move.plan_id);
    stmt.bind_text(3, move.scan_item_id);
    stmt.bind_text(4, move.source_path);
    stmt.bind_text(5, move.dest_path);
    stmt.bind_int(6, static_cast<int>(move.confidence * 100));
    stmt.bind_text(7, move.reason);
    stmt.bind_text(8, core::to_string(move.status));
    stmt.step_done();
}

void OrgMoveRepository::insert_batch(const std::vector<core::OrgMove>& moves) {
    if (moves.empty()) return;

    db_.begin_transaction();
    auto stmt = db_.prepare(R"(
        INSERT INTO org_moves (id, plan_id, scan_item_id, source_path, dest_path,
            confidence, reason, status)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");

    for (const auto& move : moves) {
        stmt.bind_text(1, move.id);
        stmt.bind_text(2, move.plan_id);
        stmt.bind_text(3, move.scan_item_id);
        stmt.bind_text(4, move.source_path);
        stmt.bind_text(5, move.dest_path);
        stmt.bind_int(6, static_cast<int>(move.confidence * 100));
        stmt.bind_text(7, move.reason);
        stmt.bind_text(8, core::to_string(move.status));
        stmt.step_done();
        stmt.reset();
    }

    db_.commit();
}

void OrgMoveRepository::update_status(const std::string& id, core::MoveStatus status) {
    auto stmt = db_.prepare("UPDATE org_moves SET status=? WHERE id=?");
    stmt.bind_text(1, core::to_string(status));
    stmt.bind_text(2, id);
    stmt.step_done();
}

std::vector<core::OrgMove> OrgMoveRepository::find_by_plan(const std::string& plan_id) {
    std::vector<core::OrgMove> moves;
    auto stmt = db_.prepare(R"(
        SELECT id, plan_id, scan_item_id, source_path, dest_path,
            confidence, reason, status
        FROM org_moves WHERE plan_id=? ORDER BY source_path
    )");
    stmt.bind_text(1, plan_id);
    while (stmt.step()) {
        moves.push_back(read_move(stmt));
    }
    return moves;
}

std::vector<core::OrgMove> OrgMoveRepository::find_by_plan_and_status(
    const std::string& plan_id, core::MoveStatus status) {
    std::vector<core::OrgMove> moves;
    auto stmt = db_.prepare(R"(
        SELECT id, plan_id, scan_item_id, source_path, dest_path,
            confidence, reason, status
        FROM org_moves WHERE plan_id=? AND status=? ORDER BY source_path
    )");
    stmt.bind_text(1, plan_id);
    stmt.bind_text(2, core::to_string(status));
    while (stmt.step()) {
        moves.push_back(read_move(stmt));
    }
    return moves;
}

std::optional<core::OrgMove> OrgMoveRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, plan_id, scan_item_id, source_path, dest_path,
            confidence, reason, status
        FROM org_moves WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_move(stmt);
    return std::nullopt;
}

int OrgMoveRepository::count_by_plan(const std::string& plan_id) {
    auto stmt = db_.prepare("SELECT COUNT(*) FROM org_moves WHERE plan_id=?");
    stmt.bind_text(1, plan_id);
    if (stmt.step()) return stmt.column_int(0);
    return 0;
}

int OrgMoveRepository::count_by_plan_and_status(const std::string& plan_id, core::MoveStatus status) {
    auto stmt = db_.prepare("SELECT COUNT(*) FROM org_moves WHERE plan_id=? AND status=?");
    stmt.bind_text(1, plan_id);
    stmt.bind_text(2, core::to_string(status));
    if (stmt.step()) return stmt.column_int(0);
    return 0;
}

core::OrgMove OrgMoveRepository::read_move(DatabaseManager::Statement& stmt) {
    core::OrgMove move;
    move.id = stmt.column_text(0);
    move.plan_id = stmt.column_text(1);
    move.scan_item_id = stmt.column_text(2);
    move.source_path = stmt.column_text(3);
    move.dest_path = stmt.column_text(4);
    move.confidence = static_cast<double>(stmt.column_int(5)) / 100.0;
    move.reason = stmt.column_text(6);
    move.status = core::move_status_from_string(stmt.column_text(7));
    return move;
}

} // namespace archive::storage
