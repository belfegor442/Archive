#include "TaxonomyRepository.h"

namespace archive::storage {

TaxonomyRepository::TaxonomyRepository(DatabaseManager& db)
    : db_(db)
{}

void TaxonomyRepository::insert(const core::TaxonomyNode& node) {
    auto stmt = db_.prepare(R"(
        INSERT INTO taxonomy_nodes (id, name, parent_id, level, file_count, icon)
        VALUES (?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, node.id);
    stmt.bind_text(2, node.name);
    stmt.bind_text(3, node.parent_id);
    stmt.bind_int(4, node.level);
    stmt.bind_int(5, node.file_count);
    stmt.bind_text(6, node.icon);
    stmt.step_done();
}

void TaxonomyRepository::update(const core::TaxonomyNode& node) {
    auto stmt = db_.prepare(R"(
        UPDATE taxonomy_nodes SET name=?, parent_id=?, level=?, file_count=?, icon=?
        WHERE id=?
    )");
    stmt.bind_text(1, node.name);
    stmt.bind_text(2, node.parent_id);
    stmt.bind_int(3, node.level);
    stmt.bind_int(4, node.file_count);
    stmt.bind_text(5, node.icon);
    stmt.bind_text(6, node.id);
    stmt.step_done();
}

std::optional<core::TaxonomyNode> TaxonomyRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, name, parent_id, level, file_count, icon
        FROM taxonomy_nodes WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_node(stmt);
    return std::nullopt;
}

std::optional<core::TaxonomyNode> TaxonomyRepository::find_by_path(const std::string& path) {
    auto stmt = db_.prepare(R"(
        SELECT id, name, parent_id, level, file_count, icon
        FROM taxonomy_nodes WHERE name=?
    )");
    stmt.bind_text(1, path);
    if (stmt.step()) return read_node(stmt);
    return std::nullopt;
}

std::vector<core::TaxonomyNode> TaxonomyRepository::find_children(const std::string& parent_id) {
    std::vector<core::TaxonomyNode> nodes;
    auto stmt = db_.prepare(R"(
        SELECT id, name, parent_id, level, file_count, icon
        FROM taxonomy_nodes WHERE parent_id=? ORDER BY name
    )");
    stmt.bind_text(1, parent_id);
    while (stmt.step()) {
        nodes.push_back(read_node(stmt));
    }
    return nodes;
}

std::vector<core::TaxonomyNode> TaxonomyRepository::find_root_nodes() {
    std::vector<core::TaxonomyNode> nodes;
    auto stmt = db_.prepare(R"(
        SELECT id, name, parent_id, level, file_count, icon
        FROM taxonomy_nodes WHERE parent_id='' ORDER BY name
    )");
    while (stmt.step()) {
        nodes.push_back(read_node(stmt));
    }
    return nodes;
}

std::vector<core::TaxonomyNode> TaxonomyRepository::find_all() {
    std::vector<core::TaxonomyNode> nodes;
    auto stmt = db_.prepare(R"(
        SELECT id, name, parent_id, level, file_count, icon
        FROM taxonomy_nodes ORDER BY level, name
    )");
    while (stmt.step()) {
        nodes.push_back(read_node(stmt));
    }
    return nodes;
}

void TaxonomyRepository::increment_count(const std::string& id) {
    auto stmt = db_.prepare("UPDATE taxonomy_nodes SET file_count=file_count+1 WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step_done();
}

void TaxonomyRepository::clear() {
    db_.execute("DELETE FROM taxonomy_nodes");
}

core::TaxonomyNode TaxonomyRepository::read_node(DatabaseManager::Statement& stmt) {
    core::TaxonomyNode node;
    node.id = stmt.column_text(0);
    node.name = stmt.column_text(1);
    node.parent_id = stmt.column_text(2);
    node.level = stmt.column_int(3);
    node.file_count = stmt.column_int(4);
    node.icon = stmt.column_text(5);
    return node;
}

} // namespace archive::storage
