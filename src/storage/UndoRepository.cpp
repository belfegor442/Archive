#include "UndoRepository.h"
#include "../core/models/UndoEntry.h"
#include "../core/enums/UndoStatus.h"

namespace archive::storage {

UndoRepository::UndoRepository(DatabaseManager& db)
    : db_(db)
{}

void UndoRepository::insert_record(const core::UndoRecord& record) {
    auto stmt = db_.prepare(R"(
        INSERT INTO undo_records (id, operation_id, moves_count, root_path, status, created_at)
        VALUES (?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, record.id);
    stmt.bind_text(2, record.operation_id);
    stmt.bind_int(3, record.moves_count);
    stmt.bind_text(4, record.root_path);
    stmt.bind_text(5, core::to_string(record.status));
    stmt.bind_text(6, record.created_at);
    stmt.step_done();
}

void UndoRepository::insert_entries(const std::vector<core::UndoEntry>& entries) {
    if (entries.empty()) return;

    db_.begin_transaction();
    auto stmt = db_.prepare(R"(
        INSERT INTO undo_entries (undo_id, source_path, dest_path, move_index)
        VALUES (?, ?, ?, ?)
    )");

    for (const auto& entry : entries) {
        stmt.bind_text(1, entry.undo_id);
        stmt.bind_text(2, entry.source_path);
        stmt.bind_text(3, entry.dest_path);
        stmt.bind_int(4, entry.move_index);
        stmt.step_done();
        stmt.reset();
    }

    db_.commit();
}

void UndoRepository::update_status(const std::string& id, core::UndoStatus status) {
    auto stmt = db_.prepare("UPDATE undo_records SET status=? WHERE id=?");
    stmt.bind_text(1, core::to_string(status));
    stmt.bind_text(2, id);
    stmt.step_done();
}

std::optional<core::UndoRecord> UndoRepository::find_record_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, operation_id, moves_count, root_path, status, created_at
        FROM undo_records WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_record(stmt);
    return std::nullopt;
}

std::vector<core::UndoRecord> UndoRepository::find_available() {
    std::vector<core::UndoRecord> records;
    auto stmt = db_.prepare(R"(
        SELECT id, operation_id, moves_count, root_path, status, created_at
        FROM undo_records WHERE status='available' ORDER BY created_at DESC
    )");
    while (stmt.step()) {
        records.push_back(read_record(stmt));
    }
    return records;
}

std::vector<core::UndoEntry> UndoRepository::find_entries(const std::string& undo_id) {
    std::vector<core::UndoEntry> entries;
    auto stmt = db_.prepare(R"(
        SELECT id, undo_id, source_path, dest_path, move_index
        FROM undo_entries WHERE undo_id=? ORDER BY move_index
    )");
    stmt.bind_text(1, undo_id);
    while (stmt.step()) {
        entries.push_back(read_entry(stmt));
    }
    return entries;
}

std::vector<core::UndoRecord> UndoRepository::find_all() {
    std::vector<core::UndoRecord> records;
    auto stmt = db_.prepare(R"(
        SELECT id, operation_id, moves_count, root_path, status, created_at
        FROM undo_records ORDER BY created_at DESC
    )");
    while (stmt.step()) {
        records.push_back(read_record(stmt));
    }
    return records;
}

core::UndoRecord UndoRepository::read_record(DatabaseManager::Statement& stmt) {
    core::UndoRecord record;
    record.id = stmt.column_text(0);
    record.operation_id = stmt.column_text(1);
    record.moves_count = stmt.column_int(2);
    record.root_path = stmt.column_text(3);
    record.status = core::undo_status_from_string(stmt.column_text(4));
    record.created_at = stmt.column_text(5);
    return record;
}

core::UndoEntry UndoRepository::read_entry(DatabaseManager::Statement& stmt) {
    core::UndoEntry entry;
    entry.undo_id = stmt.column_text(1);
    entry.source_path = stmt.column_text(2);
    entry.dest_path = stmt.column_text(3);
    entry.move_index = stmt.column_int(4);
    return entry;
}

} // namespace archive::storage
