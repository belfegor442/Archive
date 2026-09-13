#pragma once

#include <string>
#include <filesystem>

#include "../storage/DatabaseManager.h"
#include "../core/models/ArchiveItem.h"

namespace archive::filesystem {

class StorageManager {
public:
    StorageManager(const std::string& base_dir, const std::string& items_dir);

    std::string create_item_dir(const std::string& item_id);
    std::string store_file(const std::string& item_id, const std::string& source_path);
    std::string store_version(const std::string& item_id, int version, const std::string& source_path);
    std::string get_item_dir(const std::string& item_id) const;
    std::string get_item_file_dir(const std::string& item_id) const;
    std::string get_item_versions_dir(const std::string& item_id) const;
    void remove_item_dir(const std::string& item_id);
    void remove_version_file(const std::string& item_id, int version);

private:
    std::string base_dir_;
    std::string items_dir_;
};

} // namespace archive::filesystem
