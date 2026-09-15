#include "ScanItemRepository.h"

namespace archive::storage {

ScanItemRepository::ScanItemRepository(DatabaseManager& db)
    : db_(db)
{}

void ScanItemRepository::insert(const core::ScanItem& item) {
    auto stmt = db_.prepare(R"(
        INSERT INTO scan_items (id, scan_id, path, filename, extension, mime_type,
            size, role, detected_project, content_preview, checksum, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, item.id);
    stmt.bind_text(2, item.scan_id);
    stmt.bind_text(3, item.path);
    stmt.bind_text(4, item.filename);
    stmt.bind_text(5, item.extension);
    stmt.bind_text(6, item.mime_type);
    stmt.bind_int64(7, static_cast<int64_t>(item.size));
    stmt.bind_text(8, item.role);
    stmt.bind_text(9, item.detected_project);
    stmt.bind_text(10, item.content_preview);
    stmt.bind_text(11, item.checksum);
    stmt.bind_text(12, item.created_at);
    stmt.bind_text(13, item.modified_at);
    stmt.step_done();
}

void ScanItemRepository::insert_batch(const std::vector<core::ScanItem>& items) {
    if (items.empty()) return;

    db_.begin_transaction();
    auto stmt = db_.prepare(R"(
        INSERT INTO scan_items (id, scan_id, path, filename, extension, mime_type,
            size, role, detected_project, content_preview, checksum, created_at, modified_at)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    for (const auto& item : items) {
        stmt.bind_text(1, item.id);
        stmt.bind_text(2, item.scan_id);
        stmt.bind_text(3, item.path);
        stmt.bind_text(4, item.filename);
        stmt.bind_text(5, item.extension);
        stmt.bind_text(6, item.mime_type);
        stmt.bind_int64(7, static_cast<int64_t>(item.size));
        stmt.bind_text(8, item.role);
        stmt.bind_text(9, item.detected_project);
        stmt.bind_text(10, item.content_preview);
        stmt.bind_text(11, item.checksum);
        stmt.bind_text(12, item.created_at);
        stmt.bind_text(13, item.modified_at);
        stmt.step_done();
        stmt.reset();
    }

    db_.commit();
}

std::vector<core::ScanItem> ScanItemRepository::find_by_scan(const std::string& scan_id) {
    std::vector<core::ScanItem> items;
    auto stmt = db_.prepare(R"(
        SELECT id, scan_id, path, filename, extension, mime_type,
            size, role, detected_project, content_preview, checksum, created_at, modified_at
        FROM scan_items WHERE scan_id=? ORDER BY path
    )");
    stmt.bind_text(1, scan_id);
    while (stmt.step()) {
        items.push_back(read_item(stmt));
    }
    return items;
}

std::optional<core::ScanItem> ScanItemRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, scan_id, path, filename, extension, mime_type,
            size, role, detected_project, content_preview, checksum, created_at, modified_at
        FROM scan_items WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_item(stmt);
    return std::nullopt;
}

std::vector<core::ScanItem> ScanItemRepository::find_by_extension(const std::string& scan_id, const std::string& ext) {
    std::vector<core::ScanItem> items;
    auto stmt = db_.prepare(R"(
        SELECT id, scan_id, path, filename, extension, mime_type,
            size, role, detected_project, content_preview, checksum, created_at, modified_at
        FROM scan_items WHERE scan_id=? AND extension=? ORDER BY path
    )");
    stmt.bind_text(1, scan_id);
    stmt.bind_text(2, ext);
    while (stmt.step()) {
        items.push_back(read_item(stmt));
    }
    return items;
}

int ScanItemRepository::count_by_scan(const std::string& scan_id) {
    auto stmt = db_.prepare("SELECT COUNT(*) FROM scan_items WHERE scan_id=?");
    stmt.bind_text(1, scan_id);
    if (stmt.step()) return stmt.column_int(0);
    return 0;
}

void ScanItemRepository::clear_scan(const std::string& scan_id) {
    auto stmt = db_.prepare("DELETE FROM scan_items WHERE scan_id=?");
    stmt.bind_text(1, scan_id);
    stmt.step_done();
}

core::ScanItem ScanItemRepository::read_item(DatabaseManager::Statement& stmt) {
    core::ScanItem item;
    item.id = stmt.column_text(0);
    item.scan_id = stmt.column_text(1);
    item.path = stmt.column_text(2);
    item.filename = stmt.column_text(3);
    item.extension = stmt.column_text(4);
    item.mime_type = stmt.column_text(5);
    item.size = static_cast<int64_t>(stmt.column_int64(6));
    item.role = stmt.column_text(7);
    item.detected_project = stmt.column_text(8);
    item.content_preview = stmt.column_text(9);
    item.checksum = stmt.column_text(10);
    item.created_at = stmt.column_text(11);
    item.modified_at = stmt.column_text(12);
    return item;
}

} // namespace archive::storage
