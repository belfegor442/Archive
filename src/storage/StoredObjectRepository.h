#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/StoredObject.h"
#include "DatabaseManager.h"

namespace archive::storage {

class StoredObjectRepository {
public:
    explicit StoredObjectRepository(DatabaseManager& db);

    std::string insert(const core::StoredObject& obj);
    void update(const core::StoredObject& obj);
    void remove(const std::string& id);
    std::optional<core::StoredObject> find_by_id(const std::string& id);
    std::vector<core::StoredObject> find_by_item(const std::string& item_id);
    std::optional<core::StoredObject> find_by_version(const std::string& version_id);
    std::optional<core::StoredObject> find_by_path(const std::string& storage_path);

private:
    DatabaseManager& db_;
    core::StoredObject read_object(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
