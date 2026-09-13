#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/Category.h"
#include "DatabaseManager.h"

namespace archive::storage {

class CategoryRepository {
public:
    explicit CategoryRepository(DatabaseManager& db);

    std::string insert(const core::Category& category);
    void update(const core::Category& category);
    void remove(const std::string& id);
    std::optional<core::Category> find_by_id(const std::string& id);
    std::vector<core::Category> find_all();
    std::optional<core::Category> find_by_name(const std::string& name);

private:
    DatabaseManager& db_;
    core::Category read_category(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
