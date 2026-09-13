#pragma once

#include <string>
#include <vector>

#include "../core/models/Activity.h"
#include "DatabaseManager.h"

namespace archive::storage {

class ActivityRepository {
public:
    explicit ActivityRepository(DatabaseManager& db);

    std::string insert(const core::Activity& activity);
    std::vector<core::Activity> find_by_item(const std::string& item_id);
    std::vector<core::Activity> find_recent(int limit = 50);
    void remove(const std::string& id);

private:
    DatabaseManager& db_;
    core::Activity read_activity(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
