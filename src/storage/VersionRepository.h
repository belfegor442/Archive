#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/Version.h"
#include "DatabaseManager.h"

namespace archive::storage {

class VersionRepository {
public:
    explicit VersionRepository(DatabaseManager& db);

    std::string insert(const core::Version& version);
    std::vector<core::Version> find_by_item(const std::string& item_id);
    std::optional<core::Version> find_latest(const std::string& item_id);
    std::optional<core::Version> find_by_id(const std::string& id);
    void remove(const std::string& id);

private:
    DatabaseManager& db_;
    core::Version read_version(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
