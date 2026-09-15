#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/TaxonomyNode.h"
#include "DatabaseManager.h"

namespace archive::storage {

class TaxonomyRepository {
public:
    explicit TaxonomyRepository(DatabaseManager& db);

    void insert(const core::TaxonomyNode& node);
    void update(const core::TaxonomyNode& node);
    std::optional<core::TaxonomyNode> find_by_id(const std::string& id);
    std::optional<core::TaxonomyNode> find_by_path(const std::string& path);
    std::vector<core::TaxonomyNode> find_children(const std::string& parent_id);
    std::vector<core::TaxonomyNode> find_root_nodes();
    std::vector<core::TaxonomyNode> find_all();
    void increment_count(const std::string& id);
    void clear();

private:
    DatabaseManager& db_;
    core::TaxonomyNode read_node(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
