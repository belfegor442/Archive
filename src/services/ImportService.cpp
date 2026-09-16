#include "ImportService.h"
#include "../filesystem/FileUtils.h"
#include "../core/utils/Uuid.h"
#include "../storage/Transaction.h"

#include <filesystem>
#include <algorithm>

namespace archive::services {

ImportService::ImportService(
    storage::DatabaseManager& db,
    storage::ArchiveItemRepository& items,
    storage::CategoryRepository& categories,
    storage::TagRepository& tags,
    storage::ActivityRepository& activities,
    storage::VersionRepository& versions,
    storage::StoredObjectRepository& stored_objects,
    filesystem::StorageManager& storage,
    ProjectDetector& detector
) : db_(db)
  , items_(items)
  , categories_(categories)
  , tags_(tags)
  , activities_(activities)
  , versions_(versions)
  , stored_objects_(stored_objects)
  , storage_(storage)
  , detector_(detector)
{}

core::ImportResult ImportService::import(const core::ImportRequest& request) {
    core::ImportResult result;

    for (const auto& path : request.paths) {
        try {
            auto item_result = import_single(path, request.category_id);
            for (auto& item : item_result.items) {
                result.items.push_back(std::move(item));
            }
            for (auto& err : item_result.errors) {
                result.errors.push_back(std::move(err));
            }
        } catch (const std::exception& e) {
            core::ImportError err;
            err.path = path;
            err.error = e.what();
            result.errors.push_back(std::move(err));
        }
    }

    return result;
}

core::ImportResult ImportService::import_single(const std::string& path,
                                                 const std::optional<std::string>& category_id) {
    core::ImportResult result;

    if (std::filesystem::is_directory(path)) {
        return import_folder(path, category_id);
    }

    auto item = create_item_from_path(path, category_id);
    std::string item_id = item.id;
    filesystem::FilesystemTracker fs_tracker;

    try {
        storage_.create_item_dir(item_id);
        fs_tracker.track_created_dir(storage_.get_item_dir(item_id));

        std::string stored_path = storage_.store_file(item_id, path);
        fs_tracker.track_copied_file(stored_path);

        uint64_t dest_size = filesystem::FileUtils::file_size(stored_path);
        if (dest_size != item.size) {
            fs_tracker.compensate();
            throw std::runtime_error("File size mismatch after copy: expected "
                + std::to_string(item.size) + " got " + std::to_string(dest_size));
        }

        std::string copy_checksum = hashing::FileHasher::hash_file(stored_path);
        item.storage_path = stored_path;
        item.checksum = copy_checksum;

        persist_import(item, stored_path, copy_checksum, item.size,
                       "Archived file from " + path);

        result.items.push_back(std::move(item));
    } catch (const std::exception& e) {
        fs_tracker.compensate();

        core::ImportError err;
        err.path = path;
        err.error = e.what();
        result.errors.push_back(std::move(err));
    }

    return result;
}

core::ImportResult ImportService::import_folder(const std::string& path,
                                                 const std::optional<std::string>& category_id) {
    core::ImportResult result;

    auto detection = detector_.detect(path);

    auto item = create_item_from_path(path, category_id);
    item.type = core::ItemType::Folder;

    if (detection.is_project) {
        item.type = core::ItemType::Project;
        item.description = detection.project_type + " project (confidence: " + detection.confidence + ")";
    }

    std::string item_id = item.id;
    filesystem::FilesystemTracker fs_tracker;

    try {
        storage_.create_item_dir(item_id);
        fs_tracker.track_created_dir(storage_.get_item_dir(item_id));

        std::string stored_path = storage_.store_folder(item_id, path);
        item.storage_path = stored_path;

        item.file_count = filesystem::FileUtils::count_files(stored_path);
        item.size = filesystem::FileUtils::total_size(stored_path);
        item.checksum = hashing::FileHasher::hash_folder(stored_path);

        persist_import(item, stored_path, item.checksum, item.size,
                       "Archived folder from " + path
                           + (detection.is_project ? " (" + detection.project_type + ")" : "")
                           + " - " + std::to_string(item.file_count) + " files",
                       "Initial folder import");

        result.items.push_back(std::move(item));
    } catch (const std::exception& e) {
        fs_tracker.compensate();

        core::ImportError err;
        err.path = path;
        err.error = e.what();
        result.errors.push_back(std::move(err));
    }

    return result;
}

core::ArchiveItem ImportService::create_item_from_path(const std::string& path,
                                                       const std::optional<std::string>& category_id) {
    core::ArchiveItem item;
    item.id = core::utils::generate_id();
    item.name = filesystem::FileUtils::file_name(path);
    item.original_path = path;
    item.storage_path = "";
    item.size = 0;
    item.file_count = 0;
    item.category_id = category_id;
    item.created_at = core::utils::now_iso();
    item.archived_at = core::utils::now_iso();
    item.last_modified_at = core::utils::now_iso();
    item.current_version = 1;
    item.is_favorite = false;

    if (std::filesystem::is_regular_file(path)) {
        item.type = core::ItemType::File;
        item.size = filesystem::FileUtils::file_size(path);
    } else if (std::filesystem::is_directory(path)) {
        item.type = core::ItemType::Folder;
    }

    return item;
}

void ImportService::persist_import(const core::ArchiveItem& item,
                                    const std::string& storage_path,
                                    const std::string& checksum,
                                    uint64_t size,
                                    const std::string& activity_details,
                                    const std::string& version_notes) {
    storage::Transaction tx(db_);

    items_.insert(item);

    core::Version ver;
    ver.id = core::utils::generate_id();
    ver.item_id = item.id;
    ver.version_number = 1;
    ver.storage_path = storage_path;
    ver.checksum = checksum;
    ver.size = size;
    ver.notes = version_notes;
    ver.created_at = core::utils::now_iso();
    versions_.insert(ver);

    core::StoredObject so;
    so.id = core::utils::generate_id();
    so.item_id = item.id;
    so.version_id = ver.id;
    so.storage_path = storage_path;
    so.size = size;
    so.checksum = checksum;
    so.created_at = core::utils::now_iso();
    stored_objects_.insert(so);

    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item.id;
    act.action = core::ActivityAction::Imported;
    act.details = activity_details;
    act.created_at = core::utils::now_iso();
    activities_.insert(act);

    tx.commit();
}

} // namespace archive::services
