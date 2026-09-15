#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/Scan.h"
#include "DatabaseManager.h"

namespace archive::storage {

class ScanRepository {
public:
    explicit ScanRepository(DatabaseManager& db);

    void insert(const core::Scan& scan);
    void update(const core::Scan& scan);
    std::optional<core::Scan> find_by_id(const std::string& id);
    std::vector<core::Scan> find_all();
    void update_status(const std::string& id, core::ScanStatus status);
    void update_counts(const std::string& id, int files, int folders);
    void set_completed(const std::string& id, int files, int folders);

private:
    DatabaseManager& db_;
    core::Scan read_scan(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
