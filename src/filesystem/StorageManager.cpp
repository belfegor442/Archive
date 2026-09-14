#include "StorageManager.h"
#include "FileUtils.h"

#include <filesystem>
#include <stdexcept>

namespace archive::filesystem {

StorageManager::StorageManager(const std::string& base_dir, const std::string& items_dir)
    : base_dir_(base_dir)
    , items_dir_(items_dir)
    , staging_(base_dir)
{}

std::string StorageManager::create_item_dir(const std::string& item_id) {
    std::string dir = get_item_dir(item_id);
    FileUtils::create_directories(dir);
    FileUtils::create_directories(dir + "/files");
    FileUtils::create_directories(dir + "/versions");
    return dir;
}

std::string StorageManager::store_file(const std::string& item_id, const std::string& source_path) {
    std::string dest_dir = get_item_file_dir(item_id);
    return FileUtils::copy_file_safe(source_path, dest_dir);
}

std::string StorageManager::store_file_in_dir(const std::string& item_id,
                                               const std::string& source_path,
                                               const std::string& relative_path) {
    std::string dest_dir = get_item_file_dir(item_id);
    std::string dest_path = dest_dir + "/" + relative_path;
    std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
    FileUtils::copy_file(source_path, dest_path);
    return dest_path;
}

std::string StorageManager::store_version(const std::string& item_id, int version,
                                          const std::string& source_path) {
    std::string versions_dir = get_item_versions_dir(item_id);
    std::string ext = FileUtils::extension(source_path);
    std::string name = "v" + std::to_string(version);
    std::string dest = versions_dir + "/" + name + ext;
    FileUtils::copy_file(source_path, dest);
    return dest;
}

std::string StorageManager::store_folder(const std::string& item_id, const std::string& source_dir) {
    std::string dest_base = get_item_file_dir(item_id);
    std::error_code ec;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(
            source_dir, std::filesystem::directory_options::skip_permission_denied, ec)) {
        std::error_code status_ec;
        auto status = entry.status(status_ec);
        if (status_ec) continue;

        if (status.type() == std::filesystem::file_type::symlink) {
            continue;
        }

        if (entry.is_regular_file()) {
            std::string relative = std::filesystem::relative(entry.path(), source_dir).string();
            std::string dest_path = dest_base + "/" + relative;
            std::filesystem::create_directories(std::filesystem::path(dest_path).parent_path());
            FileUtils::copy_file(entry.path().string(), dest_path);
        }
    }

    if (ec) {
        throw std::runtime_error("Error iterating source folder: " + ec.message());
    }

    return dest_base;
}

std::string StorageManager::get_item_dir(const std::string& item_id) const {
    return items_dir_ + "/" + item_id;
}

std::string StorageManager::get_item_file_dir(const std::string& item_id) const {
    return get_item_dir(item_id) + "/files";
}

std::string StorageManager::get_item_versions_dir(const std::string& item_id) const {
    return get_item_dir(item_id) + "/versions";
}

void StorageManager::remove_item_dir(const std::string& item_id) {
    std::string dir = get_item_dir(item_id);
    if (std::filesystem::exists(dir)) {
        FileUtils::remove_directory(dir);
    }
}

void StorageManager::remove_version_file(const std::string& item_id, int version) {
    auto versions_dir = get_item_versions_dir(item_id);
    if (!std::filesystem::exists(versions_dir)) return;
    std::string prefix = "v" + std::to_string(version);
    for (const auto& entry : std::filesystem::directory_iterator(versions_dir)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            if (name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix) {
                FileUtils::remove_file(entry.path().string());
                break;
            }
        }
    }
}

bool StorageManager::item_dir_exists(const std::string& item_id) const {
    return std::filesystem::exists(get_item_dir(item_id));
}

bool StorageManager::version_file_exists(const std::string& item_id, int version) const {
    auto versions_dir = get_item_versions_dir(item_id);
    if (!std::filesystem::exists(versions_dir)) return false;
    std::string prefix = "v" + std::to_string(version);
    for (const auto& entry : std::filesystem::directory_iterator(versions_dir)) {
        if (entry.is_regular_file()) {
            std::string name = entry.path().filename().string();
            if (name.size() >= prefix.size() && name.substr(0, prefix.size()) == prefix) {
                return true;
            }
        }
    }
    return false;
}

} // namespace archive::filesystem
