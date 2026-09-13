#include "TagRepository.h"

namespace archive::storage {

TagRepository::TagRepository(DatabaseManager& db)
    : db_(db)
{}

std::string TagRepository::insert(const core::Tag& tag) {
    auto stmt = db_.prepare("INSERT INTO tags (id, name, color) VALUES (?, ?, ?)");
    stmt.bind_text(1, tag.id);
    stmt.bind_text(2, tag.name);
    stmt.bind_text(3, tag.color);
    stmt.step();
    return tag.id;
}

void TagRepository::update(const core::Tag& tag) {
    auto stmt = db_.prepare("UPDATE tags SET name=?, color=? WHERE id=?");
    stmt.bind_text(1, tag.name);
    stmt.bind_text(2, tag.color);
    stmt.bind_text(3, tag.id);
    stmt.step();
}

void TagRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM tags WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step();
}

std::optional<core::Tag> TagRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare("SELECT * FROM tags WHERE id=?");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_tag(stmt);
    return std::nullopt;
}

std::optional<core::Tag> TagRepository::find_by_name(const std::string& name) {
    auto stmt = db_.prepare("SELECT * FROM tags WHERE name=?");
    stmt.bind_text(1, name);
    if (stmt.step()) return read_tag(stmt);
    return std::nullopt;
}

std::vector<core::Tag> TagRepository::find_all() {
    std::vector<core::Tag> tags;
    auto stmt = db_.prepare("SELECT * FROM tags ORDER BY name");
    while (stmt.step()) {
        tags.push_back(read_tag(stmt));
    }
    return tags;
}

std::vector<core::Tag> TagRepository::find_by_item(const std::string& item_id) {
    std::vector<core::Tag> tags;
    auto stmt = db_.prepare(R"(
        SELECT t.id, t.name, t.color FROM tags t
        JOIN item_tags it ON t.id = it.tag_id
        WHERE it.item_id = ?
        ORDER BY t.name
    )");
    stmt.bind_text(1, item_id);
    while (stmt.step()) {
        tags.push_back(read_tag(stmt));
    }
    return tags;
}

core::Tag TagRepository::read_tag(DatabaseManager::Statement& stmt) {
    core::Tag tag;
    tag.id = stmt.column_text(0);
    tag.name = stmt.column_text(1);
    tag.color = stmt.column_text(2);
    return tag;
}

} // namespace archive::storage
