#pragma once

#include <string>
#include <filesystem>

#include "../storage/DatabaseManager.h"
#include "../core/models/ArchiveItem.h"
#include "StagingManager.h"
#include "FilesystemTracker.h"

namespace archive::filesystem {

class StorageManager {
public:
    StorageManager(const std::string& base_dir, const std::string& items_dir);

    std::string create_item_dir(const std::string& item_id);
    std::string store_file(const std::string& item_id, const std::string& source_path);
    std::string store_file_in_dir(const std::string& item_id, const std::string& source_path,
                                  const std::string& relative_path);
    std::string store_version(const std::string& item_id, int version, const std::string& source_path);
    std::string store_folder(const std::string& item_id, const std::string& source_dir);

    std::string get_item_dir(const std::string& item_id) const;
    std::string get_item_file_dir(const std::string& item_id) const;
    std::string get_item_versions_dir(const std::string& item_id) const;

    void remove_item_dir(const std::string& item_id);
    void remove_version_file(const std::string& item_id, int version);

    bool item_dir_exists(const std::string& item_id) const;
    bool version_file_exists(const std::string& item_id, int version) const;

    StagingManager& staging() { return staging_; }
    const StagingManager& staging() const { return staging_; }

private:
    std::string base_dir_;
    std::string items_dir_;
    StagingManager staging_;
};

} // namespace archive::filesystem
