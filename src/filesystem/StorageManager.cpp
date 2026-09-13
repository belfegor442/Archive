#include "StorageManager.h"
#include "FileUtils.h"

namespace archive::filesystem {

StorageManager::StorageManager(const std::string& base_dir, const std::string& items_dir)
    : base_dir_(base_dir)
    , items_dir_(items_dir)
{}

std::string StorageManager::create_item_dir(const std::string& item_id) {
    std::string dir = get_item_dir(item_id);
    FileUtils::create_directories(dir);
    FileUtils::create_directories(dir + "/files");
    FileUtils::create_directories(dir + "/versions");
    return dir;
}

std::string StorageManager::store_file(const std::string& item_id, const std::string& source_path) {
    std::string dest = get_item_file_dir(item_id) + "/" + FileUtils::file_name(source_path);
    FileUtils::copy_file(source_path, dest);
    return dest;
}

std::string StorageManager::store_version(const std::string& item_id, int version, const std::string& source_path) {
    std::string versions_dir = get_item_versions_dir(item_id);
    std::string ext = FileUtils::extension(source_path);
    std::string dest = versions_dir + "/v" + std::to_string(version) + ext;
    FileUtils::copy_file(source_path, dest);
    return dest;
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
    FileUtils::remove_directory(get_item_dir(item_id));
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

} // namespace archive::filesystem
