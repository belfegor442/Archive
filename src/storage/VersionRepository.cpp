#include "VersionRepository.h"

namespace archive::storage {

VersionRepository::VersionRepository(DatabaseManager& db)
    : db_(db)
{}

std::string VersionRepository::insert(const core::Version& version) {
    auto stmt = db_.prepare(R"(
        INSERT INTO versions (id, item_id, version_number, storage_path, checksum, size, notes, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, version.id);
    stmt.bind_text(2, version.item_id);
    stmt.bind_int(3, version.version_number);
    stmt.bind_text(4, version.storage_path);
    stmt.bind_text(5, version.checksum);
    stmt.bind_int64(6, static_cast<int64_t>(version.size));
    stmt.bind_text(7, version.notes);
    stmt.bind_text(8, version.created_at);
    stmt.step_done();
    return version.id;
}

std::vector<core::Version> VersionRepository::find_by_item(const std::string& item_id) {
    std::vector<core::Version> versions;
    auto stmt = db_.prepare("SELECT * FROM versions WHERE item_id=? ORDER BY version_number DESC");
    stmt.bind_text(1, item_id);
    while (stmt.step()) {
        versions.push_back(read_version(stmt));
    }
    return versions;
}

std::optional<core::Version> VersionRepository::find_latest(const std::string& item_id) {
    auto stmt = db_.prepare("SELECT * FROM versions WHERE item_id=? ORDER BY version_number DESC LIMIT 1");
    stmt.bind_text(1, item_id);
    if (stmt.step()) return read_version(stmt);
    return std::nullopt;
}

std::optional<core::Version> VersionRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare("SELECT * FROM versions WHERE id=?");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_version(stmt);
    return std::nullopt;
}

void VersionRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM versions WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step_done();
}

core::Version VersionRepository::read_version(DatabaseManager::Statement& stmt) {
    core::Version v;
    v.id = stmt.column_text(0);
    v.item_id = stmt.column_text(1);
    v.version_number = stmt.column_int(2);
    v.storage_path = stmt.column_text(3);
    v.checksum = stmt.column_text(4);
    v.size = static_cast<uint64_t>(stmt.column_int64(5));
    v.notes = stmt.column_text(6);
    v.created_at = stmt.column_text(7);
    return v;
}

} // namespace archive::storage
