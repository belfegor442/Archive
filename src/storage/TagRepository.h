#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/Tag.h"
#include "DatabaseManager.h"

namespace archive::storage {

class TagRepository {
public:
    explicit TagRepository(DatabaseManager& db);

    std::string insert(const core::Tag& tag);
    void update(const core::Tag& tag);
    void remove(const std::string& id);
    std::optional<core::Tag> find_by_id(const std::string& id);
    std::optional<core::Tag> find_by_name(const std::string& name);
    std::vector<core::Tag> find_all();
    std::vector<core::Tag> find_by_item(const std::string& item_id);

private:
    DatabaseManager& db_;
    core::Tag read_tag(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
