#include "VersionService.h"
#include "../filesystem/FileUtils.h"
#include "../core/utils/Uuid.h"

#include <filesystem>

namespace archive::services {

VersionService::VersionService(
    storage::VersionRepository& versions,
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities,
    filesystem::StorageManager& storage
) : versions_(versions)
  , items_(items)
  , activities_(activities)
  , storage_(storage)
{}

core::Version VersionService::create_version(const std::string& item_id, const std::string& file_path, const std::string& notes) {
    auto item = items_.find_by_id(item_id);
    if (!item) throw std::runtime_error("Item not found: " + item_id);

    int new_version = item->current_version + 1;
    std::string stored_path = storage_.store_version(item_id, new_version, file_path);
    std::string checksum = hashing::FileHasher::hash_file(file_path);
    uint64_t size = filesystem::FileUtils::file_size(file_path);

    core::Version ver;
    ver.id = core::utils::generate_id();
    ver.item_id = item_id;
    ver.version_number = new_version;
    ver.storage_path = stored_path;
    ver.checksum = checksum;
    ver.size = size;
    ver.notes = notes;
    ver.created_at = core::utils::now_iso();

    versions_.insert(ver);
    items_.increment_version(item_id);

    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::VersionCreated;
    act.details = "Version " + std::to_string(new_version) + " created";
    act.created_at = core::utils::now_iso();
    activities_.insert(act);

    return ver;
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

    std::string item_file_dir = storage_.get_item_file_dir(item_id);
    std::string original_name = filesystem::FileUtils::file_name(item->original_path);
    std::string dest = item_file_dir + "/" + original_name;
    std::filesystem::copy_file(ver->storage_path, dest, std::filesystem::copy_options::overwrite_existing);

    item->storage_path = dest;
    item->checksum = ver->checksum;
    item->size = ver->size;
    item->current_version = ver->version_number;
    item->last_modified_at = core::utils::now_iso();
    items_.update(*item);

    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::VersionRestored;
    act.details = "Restored to version " + std::to_string(ver->version_number);
    act.created_at = core::utils::now_iso();
    activities_.insert(act);
}

} // namespace archive::services
