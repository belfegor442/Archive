#include "ArchiveItemRepository.h"

#include "../core/enums/ItemType.h"
#include "../core/enums/ItemStatus.h"

namespace archive::storage {

ArchiveItemRepository::ArchiveItemRepository(DatabaseManager& db)
    : db_(db)
{}

std::string ArchiveItemRepository::insert(const core::ArchiveItem& item) {
    auto stmt = db_.prepare(R"(
        INSERT INTO archive_items (id, name, type, status, description, original_path,
            storage_path, size, file_count, category_id, created_at, archived_at,
            last_modified_at, checksum, current_version, is_favorite)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");

    stmt.bind_text(1, item.id);
    stmt.bind_text(2, item.name);
    stmt.bind_text(3, core::to_string(item.type));
    stmt.bind_text(4, core::to_string(item.status));
    stmt.bind_text(5, item.description);
    stmt.bind_text(6, item.original_path);
    stmt.bind_text(7, item.storage_path);
    stmt.bind_int64(8, static_cast<int64_t>(item.size));
    stmt.bind_int(9, item.file_count);

    if (item.category_id.has_value())
        stmt.bind_text(10, item.category_id.value());
    else
        stmt.bind_text_null(10);

    stmt.bind_text(11, item.created_at);
    stmt.bind_text(12, item.archived_at);
    stmt.bind_text(13, item.last_modified_at);
    stmt.bind_text(14, item.checksum);
    stmt.bind_int(15, item.current_version);
    stmt.bind_int(16, item.is_favorite ? 1 : 0);

    stmt.step_done();
    return item.id;
}

void ArchiveItemRepository::update(const core::ArchiveItem& item) {
    auto stmt = db_.prepare(R"(
        UPDATE archive_items SET name=?, type=?, status=?, description=?,
            original_path=?, storage_path=?, size=?, file_count=?, category_id=?,
            last_modified_at=?, checksum=?, current_version=?, is_favorite=?
        WHERE id=?
    )");

    stmt.bind_text(1, item.name);
    stmt.bind_text(2, core::to_string(item.type));
    stmt.bind_text(3, core::to_string(item.status));
    stmt.bind_text(4, item.description);
    stmt.bind_text(5, item.original_path);
    stmt.bind_text(6, item.storage_path);
    stmt.bind_int64(7, static_cast<int64_t>(item.size));
    stmt.bind_int(8, item.file_count);

    if (item.category_id.has_value())
        stmt.bind_text(9, item.category_id.value());
    else
        stmt.bind_text_null(9);

    stmt.bind_text(10, item.last_modified_at);
    stmt.bind_text(11, item.checksum);
    stmt.bind_int(12, item.current_version);
    stmt.bind_int(13, item.is_favorite ? 1 : 0);
    stmt.bind_text(14, item.id);

    stmt.step_done();
}

void ArchiveItemRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM archive_items WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step_done();
}

std::optional<core::ArchiveItem> ArchiveItemRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare("SELECT * FROM archive_items WHERE id=?");
    stmt.bind_text(1, id);

    if (stmt.step()) {
        auto item = read_item(stmt);
        load_tags(item);
        return item;
    }
    return std::nullopt;
}

std::vector<core::ArchiveItem> ArchiveItemRepository::find_all() {
    std::vector<core::ArchiveItem> items;
    auto stmt = db_.prepare("SELECT * FROM archive_items ORDER BY archived_at DESC");
    while (stmt.step()) {
        auto item = read_item(stmt);
        load_tags(item);
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<core::ArchiveItem> ArchiveItemRepository::find_by_status(core::ItemStatus status) {
    std::vector<core::ArchiveItem> items;
    auto stmt = db_.prepare("SELECT * FROM archive_items WHERE status=? ORDER BY archived_at DESC");
    stmt.bind_text(1, core::to_string(status));
    while (stmt.step()) {
        auto item = read_item(stmt);
        load_tags(item);
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<core::ArchiveItem> ArchiveItemRepository::find_by_category(const std::string& category_id) {
    std::vector<core::ArchiveItem> items;
    auto stmt = db_.prepare("SELECT * FROM archive_items WHERE category_id=? ORDER BY archived_at DESC");
    stmt.bind_text(1, category_id);
    while (stmt.step()) {
        auto item = read_item(stmt);
        load_tags(item);
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<core::ArchiveItem> ArchiveItemRepository::find_favorites() {
    std::vector<core::ArchiveItem> items;
    auto stmt = db_.prepare("SELECT * FROM archive_items WHERE is_favorite=1 ORDER BY archived_at DESC");
    while (stmt.step()) {
        auto item = read_item(stmt);
        load_tags(item);
        items.push_back(std::move(item));
    }
    return items;
}

std::vector<core::ArchiveItem> ArchiveItemRepository::search(const std::string& query) {
    std::vector<core::ArchiveItem> items;
    std::string pattern = "%" + query + "%";
    auto stmt = db_.prepare(R"(
        SELECT * FROM archive_items
        WHERE name LIKE ? OR description LIKE ? OR original_path LIKE ?
        ORDER BY archived_at DESC
    )");
    stmt.bind_text(1, pattern);
    stmt.bind_text(2, pattern);
    stmt.bind_text(3, pattern);
    while (stmt.step()) {
        auto item = read_item(stmt);
        load_tags(item);
        items.push_back(std::move(item));
    }
    return items;
}

int ArchiveItemRepository::count() {
    auto stmt = db_.prepare("SELECT COUNT(*) FROM archive_items");
    if (stmt.step()) return stmt.column_int(0);
    return 0;
}

int ArchiveItemRepository::count_by_status(core::ItemStatus status) {
    auto stmt = db_.prepare("SELECT COUNT(*) FROM archive_items WHERE status=?");
    stmt.bind_text(1, core::to_string(status));
    if (stmt.step()) return stmt.column_int(0);
    return 0;
}

void ArchiveItemRepository::update_status(const std::string& id, core::ItemStatus status) {
    auto stmt = db_.prepare("UPDATE archive_items SET status=? WHERE id=?");
    stmt.bind_text(1, core::to_string(status));
    stmt.bind_text(2, id);
    stmt.step_done();
}

void ArchiveItemRepository::set_favorite(const std::string& id, bool favorite) {
    auto stmt = db_.prepare("UPDATE archive_items SET is_favorite=? WHERE id=?");
    stmt.bind_int(1, favorite ? 1 : 0);
    stmt.bind_text(2, id);
    stmt.step_done();
}

void ArchiveItemRepository::update_category(const std::string& id, const std::string& category_id) {
    auto stmt = db_.prepare("UPDATE archive_items SET category_id=? WHERE id=?");
    stmt.bind_text(1, category_id);
    stmt.bind_text(2, id);
    stmt.step_done();
}

void ArchiveItemRepository::increment_version(const std::string& id) {
    auto stmt = db_.prepare("UPDATE archive_items SET current_version=current_version+1 WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step_done();
}

std::vector<core::Tag> ArchiveItemRepository::get_tags(const std::string& item_id) {
    std::vector<core::Tag> tags;
    auto stmt = db_.prepare(R"(
        SELECT t.id, t.name, t.color FROM tags t
        JOIN item_tags it ON t.id = it.tag_id
        WHERE it.item_id = ?
        ORDER BY t.name
    )");
    stmt.bind_text(1, item_id);
    while (stmt.step()) {
        core::Tag tag;
        tag.id = stmt.column_text(0);
        tag.name = stmt.column_text(1);
        tag.color = stmt.column_text(2);
        tags.push_back(std::move(tag));
    }
    return tags;
}

void ArchiveItemRepository::add_tag(const std::string& item_id, const std::string& tag_id) {
    auto stmt = db_.prepare("INSERT OR IGNORE INTO item_tags (item_id, tag_id) VALUES (?, ?)");
    stmt.bind_text(1, item_id);
    stmt.bind_text(2, tag_id);
    stmt.step_done();
}

void ArchiveItemRepository::remove_tag(const std::string& item_id, const std::string& tag_id) {
    auto stmt = db_.prepare("DELETE FROM item_tags WHERE item_id=? AND tag_id=?");
    stmt.bind_text(1, item_id);
    stmt.bind_text(2, tag_id);
    stmt.step_done();
}

core::DashboardStats ArchiveItemRepository::get_stats() {
    core::DashboardStats stats;

    auto stmt_total = db_.prepare("SELECT COUNT(*) FROM archive_items WHERE status != 'deleted'");
    if (stmt_total.step()) stats.total_items = stmt_total.column_int(0);

    auto stmt_archived = db_.prepare("SELECT COUNT(*) FROM archive_items WHERE status='archived'");
    if (stmt_archived.step()) stats.archived_items = stmt_archived.column_int(0);

    auto stmt_fav = db_.prepare("SELECT COUNT(*) FROM archive_items WHERE is_favorite=1");
    if (stmt_fav.step()) stats.favorite_items = stmt_fav.column_int(0);

    auto stmt_deleted = db_.prepare("SELECT COUNT(*) FROM archive_items WHERE status='deleted'");
    if (stmt_deleted.step()) stats.deleted_items = stmt_deleted.column_int(0);

    auto stmt_size = db_.prepare("SELECT COALESCE(SUM(size), 0) FROM archive_items WHERE status='archived'");
    if (stmt_size.step()) stats.total_size = static_cast<uint64_t>(stmt_size.column_int64(0));

    auto stmt_recent = db_.prepare("SELECT * FROM archive_items WHERE status='archived' ORDER BY archived_at DESC LIMIT 10");
    while (stmt_recent.step()) {
        stats.recent_items.push_back(read_item(stmt_recent));
    }

    return stats;
}

core::ArchiveItem ArchiveItemRepository::read_item(DatabaseManager::Statement& stmt) {
    core::ArchiveItem item;
    item.id = stmt.column_text(0);
    item.name = stmt.column_text(1);
    item.type = core::item_type_from_string(stmt.column_text(2));
    item.status = core::item_status_from_string(stmt.column_text(3));
    item.description = stmt.column_text(4);
    item.original_path = stmt.column_text(5);
    item.storage_path = stmt.column_text(6);
    item.size = static_cast<uint64_t>(stmt.column_int64(7));
    item.file_count = stmt.column_int(8);

    if (!stmt.column_is_null(9))
        item.category_id = stmt.column_text(9);

    item.created_at = stmt.column_text(10);
    item.archived_at = stmt.column_text(11);
    item.last_modified_at = stmt.column_text(12);
    item.checksum = stmt.column_text(13);
    item.current_version = stmt.column_int(14);
    item.is_favorite = stmt.column_int(15) != 0;

    return item;
}

void ArchiveItemRepository::load_tags(core::ArchiveItem& item) {
    item.tags = get_tags(item.id);
}

} // namespace archive::storage
