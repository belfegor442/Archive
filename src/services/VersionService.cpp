#include "VersionService.h"
#include "../filesystem/FileUtils.h"
#include "../core/utils/Uuid.h"
#include "../storage/Transaction.h"

#include <filesystem>

namespace archive::services {

VersionService::VersionService(
    storage::DatabaseManager& db,
    storage::VersionRepository& versions,
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities,
    storage::StoredObjectRepository& stored_objects,
    filesystem::StorageManager& storage
) : db_(db)
  , versions_(versions)
  , items_(items)
  , activities_(activities)
  , stored_objects_(stored_objects)
  , storage_(storage)
{}

core::Version VersionService::create_version(const std::string& item_id, const std::string& file_path,
                                              const std::string& notes) {
    auto item = items_.find_by_id(item_id);
    if (!item) throw std::runtime_error("Item not found: " + item_id);

    int new_version = item->current_version + 1;
    filesystem::FilesystemTracker fs_tracker;

    try {
        std::string stored_path = storage_.store_version(item_id, new_version, file_path);
        fs_tracker.track_copied_file(stored_path);

        std::string checksum;
        uint64_t size = 0;
        if (item->type == core::ItemType::File) {
            checksum = hashing::FileHasher::hash_file(file_path);
            size = filesystem::FileUtils::file_size(file_path);
        } else {
            checksum = hashing::FileHasher::hash_folder(file_path);
            size = filesystem::FileUtils::total_size(file_path);
        }

        core::Version ver;
        ver.id = core::utils::generate_id();
        ver.item_id = item_id;
        ver.version_number = new_version;
        ver.storage_path = stored_path;
        ver.checksum = checksum;
        ver.size = size;
        ver.notes = notes;
        ver.created_at = core::utils::now_iso();

        {
            storage::Transaction tx(db_);

            versions_.insert(ver);
            items_.increment_version(item_id);

            core::StoredObject so;
            so.id = core::utils::generate_id();
            so.item_id = item_id;
            so.version_id = ver.id;
            so.storage_path = stored_path;
            so.size = size;
            so.checksum = checksum;
            so.created_at = core::utils::now_iso();
            stored_objects_.insert(so);

            core::Activity act;
            act.id = core::utils::generate_id();
            act.item_id = item_id;
            act.action = core::ActivityAction::VersionCreated;
            act.details = "Version " + std::to_string(new_version) + " created";
            act.created_at = core::utils::now_iso();
            activities_.insert(act);

            tx.commit();
        }

        return ver;
    } catch (const std::exception& e) {
        fs_tracker.compensate();
        throw;
    }
}

std::vector<core::Version> VersionService::get_versions(const std::string& item_id) {
    return versions_.find_by_item(item_id);
}

std::optional<core::Version> VersionService::get_latest(const std::string& item_id) {
    return versions_.find_latest(item_id);
}

void VersionService::restore(const std::string& item_id, const std::string& version_id) {
    auto ver = versions_.find_by_id(version_id);
    if (!ver) throw std::runtime_error("Version not found: " + version_id);

    auto item = items_.find_by_id(item_id);
    if (!item) throw std::runtime_error("Item not found: " + item_id);

    if (!std::filesystem::exists(ver->storage_path)) {
        throw std::runtime_error("Version file missing: " + ver->storage_path);
    }

    filesystem::FilesystemTracker fs_tracker;

    try {
        std::string item_file_dir = storage_.get_item_file_dir(item_id);
        std::string original_name = filesystem::FileUtils::file_name(item->original_path);
        std::string dest = item_file_dir + "/" + original_name;
        if (std::filesystem::exists(dest)) {
            dest = filesystem::FileUtils::unique_path(item_file_dir,
                filesystem::FileUtils::stem(item->original_path) + "_v" + std::to_string(ver->version_number),
                filesystem::FileUtils::extension(item->original_path));
        }
        std::filesystem::copy_file(ver->storage_path, dest);
        fs_tracker.track_copied_file(dest);

        {
            storage::Transaction tx(db_);

            item->storage_path = dest;
            item->checksum = ver->checksum;
            item->size = ver->size;
            item->current_version = ver->version_number;
            item->last_modified_at = core::utils::now_iso();
            items_.update(*item);

            auto existing_so = stored_objects_.find_by_version(ver->id);
            if (existing_so) {
                existing_so->storage_path = dest;
                existing_so->size = ver->size;
                existing_so->checksum = ver->checksum;
                stored_objects_.update(*existing_so);
            } else {
                core::StoredObject so;
                so.id = core::utils::generate_id();
                so.item_id = item_id;
                so.version_id = ver->id;
                so.storage_path = dest;
                so.size = ver->size;
                so.checksum = ver->checksum;
                so.created_at = core::utils::now_iso();
                stored_objects_.insert(so);
            }

            core::Activity act;
            act.id = core::utils::generate_id();
            act.item_id = item_id;
            act.action = core::ActivityAction::VersionRestored;
            act.details = "Restored to version " + std::to_string(ver->version_number);
            act.created_at = core::utils::now_iso();
            activities_.insert(act);

            tx.commit();
        }
    } catch (const std::exception& e) {
        fs_tracker.compensate();
        throw;
    }
}

} // namespace archive::services
