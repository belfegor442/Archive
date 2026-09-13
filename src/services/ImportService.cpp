#include "ImportService.h"
#include "../filesystem/FileUtils.h"
#include "../core/utils/Uuid.h"

#include <filesystem>
#include <algorithm>

namespace archive::services {

ImportService::ImportService(
    storage::ArchiveItemRepository& items,
    storage::CategoryRepository& categories,
    storage::TagRepository& tags,
    storage::ActivityRepository& activities,
    storage::VersionRepository& versions,
    filesystem::StorageManager& storage,
    ProjectDetector& detector
) : items_(items)
  , categories_(categories)
  , tags_(tags)
  , activities_(activities)
  , versions_(versions)
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

core::ImportResult ImportService::import_single(const std::string& path, const std::optional<std::string>& category_id) {
    core::ImportResult result;

    if (!std::filesystem::exists(path)) {
        core::ImportError err;
        err.path = path;
        err.error = "Path does not exist";
        result.errors.push_back(std::move(err));
        return result;
    }

    if (std::filesystem::is_directory(path)) {
        return import_folder(path, category_id);
    }

    try {
        auto item = create_item_from_path(path, category_id);
        storage_.create_item_dir(item.id);
        std::string stored_path = storage_.store_file(item.id, path);
        item.storage_path = stored_path;
        item.checksum = hashing::FileHasher::hash_file(path);
        items_.insert(item);

        core::Version ver;
        ver.id = core::utils::generate_id();
        ver.item_id = item.id;
        ver.version_number = 1;
        ver.storage_path = stored_path;
        ver.checksum = item.checksum;
        ver.size = item.size;
        ver.created_at = core::utils::now_iso();
        versions_.insert(ver);

        core::Activity act;
        act.id = core::utils::generate_id();
        act.item_id = item.id;
        act.action = core::ActivityAction::Imported;
        act.details = "Archived from " + path;
        act.created_at = core::utils::now_iso();
        activities_.insert(act);

        result.items.push_back(std::move(item));
    } catch (const std::exception& e) {
        core::ImportError err;
        err.path = path;
        err.error = e.what();
        result.errors.push_back(std::move(err));
    }

    return result;
}

core::ImportResult ImportService::import_folder(const std::string& path, const std::optional<std::string>& category_id) {
    core::ImportResult result;

    auto detection = detector_.detect(path);

    auto item = create_item_from_path(path, category_id);
    item.type = core::ItemType::Folder;

    if (detection.is_project) {
        item.type = core::ItemType::Project;
        item.description = detection.project_type + " project (confidence: " + detection.confidence + ")";
    }

    storage_.create_item_dir(item.id);
    item.file_count = filesystem::FileUtils::count_files(path);
    item.size = 0;
    item.checksum = "";

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                item.size += entry.file_size();
            }
        }
    } catch (...) {
        item.size = 0;
    }

    item.storage_path = storage_.get_item_file_dir(item.id);
    items_.insert(item);

    int files_copied = 0;
    int files_failed = 0;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                try {
                    std::string relative = std::filesystem::relative(entry.path(), path).string();
                    std::string dest_dir = storage_.get_item_file_dir(item.id);
                    std::string dest_path = dest_dir + "/" + relative;
                    std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
                    std::filesystem::copy_file(entry.path(), dest_path, std::filesystem::copy_options::overwrite_existing);
                    files_copied++;
                } catch (...) {
                    files_failed++;
                }
            }
        }
    } catch (...) {
        files_failed++;
    }

    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item.id;
    act.action = core::ActivityAction::Imported;
    act.details = "Archived folder from " + path
        + (detection.is_project ? " (" + detection.project_type + ")" : "")
        + " - " + std::to_string(files_copied) + " files copied"
        + (files_failed > 0 ? ", " + std::to_string(files_failed) + " failed" : "");
    act.created_at = core::utils::now_iso();
    activities_.insert(act);

    if (files_failed > 0) {
        core::ImportError err;
        err.path = path;
        err.error = std::to_string(files_failed) + " files failed to copy";
        result.errors.push_back(std::move(err));
    }

    result.items.push_back(std::move(item));
    return result;
}

core::ArchiveItem ImportService::create_item_from_path(const std::string& path, const std::optional<std::string>& category_id) {
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

} // namespace archive::services
