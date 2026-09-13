#include "ImportService.h"
#include "../filesystem/FileUtils.h"

#include <filesystem>
#include <random>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <algorithm>

namespace archive::services {

ImportService::ImportService(
    storage::ArchiveItemRepository& items,
    storage::CategoryRepository& categories,
    storage::TagRepository& tags,
    storage::ActivityRepository& activities,
    filesystem::StorageManager& storage,
    ProjectDetector& detector
) : items_(items)
  , categories_(categories)
  , tags_(tags)
  , activities_(activities)
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

        core::Activity act;
        act.id = generate_id();
        act.item_id = item.id;
        act.action = core::ActivityAction::Imported;
        act.details = "Archived from " + path;
        act.created_at = now_iso();
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
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
            if (entry.is_regular_file()) {
                item.size += entry.file_size();
            }
        }
    } catch (...) {
        item.size = 0;
    }

    item.checksum = "";
    items_.insert(item);

    core::Activity act;
    act.id = generate_id();
    act.item_id = item.id;
    act.action = core::ActivityAction::Imported;
    act.details = "Archived folder from " + path + (detection.is_project ? " (" + detection.project_type + ")" : "");
    act.created_at = now_iso();
    activities_.insert(act);

    result.items.push_back(std::move(item));
    return result;
}

core::ArchiveItem ImportService::create_item_from_path(const std::string& path, const std::optional<std::string>& category_id) {
    core::ArchiveItem item;
    item.id = generate_id();
    item.name = filesystem::FileUtils::file_name(path);
    item.original_path = path;
    item.storage_path = "";
    item.size = 0;
    item.file_count = 0;
    item.category_id = category_id;
    item.created_at = now_iso();
    item.archived_at = now_iso();
    item.last_modified_at = now_iso();
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

std::string ImportService::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(
        0, std::numeric_limits<uint64_t>::max()
    );

    std::ostringstream oss;
    oss << std::hex << dis(gen) << dis(gen);
    std::string id = oss.str();
    id.resize(32);
    return id;
}

std::string ImportService::now_iso() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);

    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace archive::services
