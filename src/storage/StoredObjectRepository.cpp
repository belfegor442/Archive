#include "StoredObjectRepository.h"

namespace archive::storage {

StoredObjectRepository::StoredObjectRepository(DatabaseManager& db)
    : db_(db)
{}

std::string StoredObjectRepository::insert(const core::StoredObject& obj) {
    auto stmt = db_.prepare(R"(
        INSERT INTO stored_objects (id, item_id, version_id, storage_path, size, checksum, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, obj.id);
    stmt.bind_text(2, obj.item_id);
    if (obj.version_id.empty())
        stmt.bind_text_null(3);
    else
        stmt.bind_text(3, obj.version_id);
    stmt.bind_text(4, obj.storage_path);
    stmt.bind_int64(5, static_cast<int64_t>(obj.size));
    stmt.bind_text(6, obj.checksum);
    stmt.bind_text(7, obj.created_at);
    stmt.step_done();
    return obj.id;
}

void StoredObjectRepository::update(const core::StoredObject& obj) {
    auto stmt = db_.prepare(R"(
        UPDATE stored_objects SET item_id=?, version_id=?, storage_path=?,
            size=?, checksum=?, created_at=?
        WHERE id=?
    )");
    stmt.bind_text(1, obj.item_id);
    if (obj.version_id.empty())
        stmt.bind_text_null(2);
    else
        stmt.bind_text(2, obj.version_id);
    stmt.bind_text(3, obj.storage_path);
    stmt.bind_int64(4, static_cast<int64_t>(obj.size));
    stmt.bind_text(5, obj.checksum);
    stmt.bind_text(6, obj.created_at);
    stmt.bind_text(7, obj.id);
    stmt.step_done();
}

void StoredObjectRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM stored_objects WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step_done();
}

std::optional<core::StoredObject> StoredObjectRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, item_id, COALESCE(version_id, ''), storage_path, size, checksum, created_at
        FROM stored_objects WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_object(stmt);
    return std::nullopt;
}

std::vector<core::StoredObject> StoredObjectRepository::find_by_item(const std::string& item_id) {
    std::vector<core::StoredObject> objects;
    auto stmt = db_.prepare(R"(
        SELECT id, item_id, COALESCE(version_id, ''), storage_path, size, checksum, created_at
        FROM stored_objects WHERE item_id=? ORDER BY created_at DESC
    )");
    stmt.bind_text(1, item_id);
    while (stmt.step()) {
        objects.push_back(read_object(stmt));
    }
    return objects;
}

std::optional<core::StoredObject> StoredObjectRepository::find_by_version(const std::string& version_id) {
    auto stmt = db_.prepare(R"(
        SELECT id, item_id, COALESCE(version_id, ''), storage_path, size, checksum, created_at
        FROM stored_objects WHERE version_id=?
    )");
    stmt.bind_text(1, version_id);
    if (stmt.step()) return read_object(stmt);
    return std::nullopt;
}

std::optional<core::StoredObject> StoredObjectRepository::find_by_path(const std::string& storage_path) {
    auto stmt = db_.prepare(R"(
        SELECT id, item_id, COALESCE(version_id, ''), storage_path, size, checksum, created_at
        FROM stored_objects WHERE storage_path=?
    )");
    stmt.bind_text(1, storage_path);
    if (stmt.step()) return read_object(stmt);
    return std::nullopt;
}

core::StoredObject StoredObjectRepository::read_object(DatabaseManager::Statement& stmt) {
    core::StoredObject obj;
    obj.id = stmt.column_text(0);
    obj.item_id = stmt.column_text(1);
    obj.version_id = stmt.column_text(2);
    obj.storage_path = stmt.column_text(3);
    obj.size = static_cast<uint64_t>(stmt.column_int64(4));
    obj.checksum = stmt.column_text(5);
    obj.created_at = stmt.column_text(6);
    return obj;
}

} // namespace archive::storage
