#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/Classification.h"
#include "DatabaseManager.h"

namespace archive::storage {

class ClassificationRepository {
public:
    explicit ClassificationRepository(DatabaseManager& db);

    void insert(const core::Classification& cls);
    void insert_batch(const std::vector<core::Classification>& items);
    std::vector<core::Classification> find_by_scan_item(const std::string& item_id);
    std::vector<core::Classification> find_by_scan(const std::string& scan_id);
    std::optional<core::Classification> find_best(const std::string& item_id);
    void clear_scan(const std::string& scan_id);

private:
    DatabaseManager& db_;
    core::Classification read_classification(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
