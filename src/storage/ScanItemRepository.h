#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/ScanItem.h"
#include "DatabaseManager.h"

namespace archive::storage {

class ScanItemRepository {
public:
    explicit ScanItemRepository(DatabaseManager& db);

    void insert(const core::ScanItem& item);
    void insert_batch(const std::vector<core::ScanItem>& items);
    std::vector<core::ScanItem> find_by_scan(const std::string& scan_id);
    std::optional<core::ScanItem> find_by_id(const std::string& id);
    std::vector<core::ScanItem> find_by_extension(const std::string& scan_id, const std::string& ext);
    int count_by_scan(const std::string& scan_id);
    void clear_scan(const std::string& scan_id);

private:
    DatabaseManager& db_;
    core::ScanItem read_item(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
