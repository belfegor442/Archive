#pragma once

#include <string>
#include <vector>

#include "../core/models/Version.h"
#include "../storage/VersionRepository.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/ActivityRepository.h"
#include "../hashing/FileHasher.h"
#include "../filesystem/StorageManager.h"

namespace archive::services {

class VersionService {
public:
    VersionService(
        storage::VersionRepository& versions,
        storage::ArchiveItemRepository& items,
        storage::ActivityRepository& activities,
        filesystem::StorageManager& storage
    );

    core::Version create_version(const std::string& item_id, const std::string& file_path, const std::string& notes = "");
    std::vector<core::Version> get_versions(const std::string& item_id);
    std::optional<core::Version> get_latest(const std::string& item_id);
    void restore(const std::string& item_id, const std::string& version_id);

private:
    storage::VersionRepository& versions_;
    storage::ArchiveItemRepository& items_;
    storage::ActivityRepository& activities_;
    filesystem::StorageManager& storage_;

    std::string generate_id();
    std::string now_iso();
};

} // namespace archive::services
