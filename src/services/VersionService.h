#pragma once

#include <string>
#include <vector>

#include "../core/models/Version.h"
#include "../storage/DatabaseManager.h"
#include "../storage/VersionRepository.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/ActivityRepository.h"
#include "../storage/StoredObjectRepository.h"
#include "../hashing/FileHasher.h"
#include "../filesystem/StorageManager.h"
#include "../filesystem/FilesystemTracker.h"

namespace archive::services {

class VersionService {
public:
    VersionService(
        storage::DatabaseManager& db,
        storage::VersionRepository& versions,
        storage::ArchiveItemRepository& items,
        storage::ActivityRepository& activities,
        storage::StoredObjectRepository& stored_objects,
        filesystem::StorageManager& storage
    );

    core::Version create_version(const std::string& item_id, const std::string& file_path,
                                 const std::string& notes = "");
    std::vector<core::Version> get_versions(const std::string& item_id);
    std::optional<core::Version> get_latest(const std::string& item_id);
    void restore(const std::string& item_id, const std::string& version_id);

private:
    storage::DatabaseManager& db_;
    storage::VersionRepository& versions_;
    storage::ArchiveItemRepository& items_;
    storage::ActivityRepository& activities_;
    storage::StoredObjectRepository& stored_objects_;
    filesystem::StorageManager& storage_;
};

} // namespace archive::services
