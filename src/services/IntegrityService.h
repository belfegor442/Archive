#pragma once

#include <string>
#include <vector>

#include "../core/types/VerificationResult.h"
#include "../core/types/ConsistencyReport.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/VersionRepository.h"
#include "../storage/StoredObjectRepository.h"
#include "../storage/ActivityRepository.h"
#include "../filesystem/StorageManager.h"
#include "../hashing/FileHasher.h"

namespace archive::services {

class IntegrityService {
public:
    IntegrityService(
        storage::ArchiveItemRepository& items,
        storage::VersionRepository& versions,
        storage::StoredObjectRepository& stored_objects,
        storage::ActivityRepository& activities,
        filesystem::StorageManager& storage
    );

    core::VerificationResult verify_all();
    core::VerificationResult verify_item(const std::string& item_id);
    core::VerificationResult verify_version(const std::string& version_id);
    core::VerificationResult verify_stored_object(const std::string& object_id);

    core::IntegrityState get_state(const std::string& item_id);
    bool is_item_valid(const std::string& item_id);

    core::ConsistencyReport check_consistency();

private:
    storage::ArchiveItemRepository& items_;
    storage::VersionRepository& versions_;
    storage::StoredObjectRepository& stored_objects_;
    storage::ActivityRepository& activities_;
    filesystem::StorageManager& storage_;

    core::VerificationItem verify_item_checksum(const core::ArchiveItem& item);
};

} // namespace archive::services
