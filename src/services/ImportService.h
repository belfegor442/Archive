#pragma once

#include <string>
#include <vector>
#include <functional>
#include <optional>

#include "../core/models/ArchiveItem.h"
#include "../core/types/ImportRequest.h"
#include "../core/types/ImportResult.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/CategoryRepository.h"
#include "../storage/TagRepository.h"
#include "../storage/ActivityRepository.h"
#include "../storage/VersionRepository.h"
#include "../storage/StoredObjectRepository.h"
#include "../hashing/FileHasher.h"
#include "../filesystem/StorageManager.h"
#include "../filesystem/FilesystemTracker.h"
#include "ProjectDetector.h"

namespace archive::services {

class ImportService {
public:
    ImportService(
        storage::DatabaseManager& db,
        storage::ArchiveItemRepository& items,
        storage::CategoryRepository& categories,
        storage::TagRepository& tags,
        storage::ActivityRepository& activities,
        storage::VersionRepository& versions,
        storage::StoredObjectRepository& stored_objects,
        filesystem::StorageManager& storage,
        ProjectDetector& detector
    );

    core::ImportResult import(const core::ImportRequest& request);
    core::ImportResult import_single(const std::string& path, const std::optional<std::string>& category_id);
    core::ImportResult import_folder(const std::string& path, const std::optional<std::string>& category_id);

private:
    storage::DatabaseManager& db_;
    storage::ArchiveItemRepository& items_;
    storage::CategoryRepository& categories_;
    storage::TagRepository& tags_;
    storage::ActivityRepository& activities_;
    storage::VersionRepository& versions_;
    storage::StoredObjectRepository& stored_objects_;
    filesystem::StorageManager& storage_;
    ProjectDetector& detector_;

    core::ArchiveItem create_item_from_path(const std::string& path, const std::optional<std::string>& category_id);
    void persist_import(const core::ArchiveItem& item,
                        const std::string& storage_path,
                        const std::string& checksum,
                        uint64_t size,
                        const std::string& activity_details,
                        const std::string& version_notes = "");
};

} // namespace archive::services
