#include "CategoryRepository.h"

namespace archive::storage {

CategoryRepository::CategoryRepository(DatabaseManager& db)
    : db_(db)
{}

std::string CategoryRepository::insert(const core::Category& category) {
    auto stmt = db_.prepare(R"(
        INSERT INTO categories (id, name, description, color, icon, parent_id, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, category.id);
    stmt.bind_text(2, category.name);
    stmt.bind_text(3, category.description);
    stmt.bind_text(4, category.color);
    stmt.bind_text(5, category.icon);
    if (category.parent_id.has_value())
        stmt.bind_text(6, category.parent_id.value());
    else
        stmt.bind_text_null(6);
    stmt.bind_text(7, category.created_at);
    stmt.step();
    return category.id;
}

void CategoryRepository::update(const core::Category& category) {
    auto stmt = db_.prepare(R"(
        UPDATE categories SET name=?, description=?, color=?, icon=?, parent_id=? WHERE id=?
    )");
    stmt.bind_text(1, category.name);
    stmt.bind_text(2, category.description);
    stmt.bind_text(3, category.color);
    stmt.bind_text(4, category.icon);
    if (category.parent_id.has_value())
        stmt.bind_text(5, category.parent_id.value());
    else
        stmt.bind_text_null(5);
    stmt.bind_text(6, category.id);
    stmt.step();
}

void CategoryRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM categories WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step();
}

std::optional<core::Category> CategoryRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare("SELECT * FROM categories WHERE id=?");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_category(stmt);
    return std::nullopt;
}

std::vector<core::Category> CategoryRepository::find_all() {
    std::vector<core::Category> categories;
    auto stmt = db_.prepare("SELECT * FROM categories ORDER BY name");
    while (stmt.step()) {
        categories.push_back(read_category(stmt));
    }
    return categories;
}

std::optional<core::Category> CategoryRepository::find_by_name(const std::string& name) {
    auto stmt = db_.prepare("SELECT * FROM categories WHERE name=?");
    stmt.bind_text(1, name);
    if (stmt.step()) return read_category(stmt);
    return std::nullopt;
}

core::Category CategoryRepository::read_category(DatabaseManager::Statement& stmt) {
    core::Category cat;
    cat.id = stmt.column_text(0);
    cat.name = stmt.column_text(1);
    cat.description = stmt.column_text(2);
    cat.color = stmt.column_text(3);
    cat.icon = stmt.column_text(4);
    if (!stmt.column_is_null(5))
        cat.parent_id = stmt.column_text(5);
    cat.created_at = stmt.column_text(6);
    return cat;
}

} // namespace archive::storage
