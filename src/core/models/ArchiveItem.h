#pragma once

#include <string>
#include <optional>
#include <vector>
#include <cstdint>

#include "../enums/ItemType.h"
#include "../enums/ItemStatus.h"
#include "Category.h"
#include "Tag.h"

namespace archive::core {

struct ArchiveItem {
    std::string id;
    std::string name;
    ItemType type = ItemType::File;
    ItemStatus status = ItemStatus::Archived;
    std::string description;
    std::string original_path;
    std::string storage_path;
    uint64_t size = 0;
    int file_count = 0;
    std::optional<std::string> category_id;
    std::string created_at;
    std::string archived_at;
    std::string last_modified_at;
    std::string checksum;
    int current_version = 1;
    bool is_favorite = false;

    // Expanded fields (not stored in archive_items table directly)
    std::optional<Category> category;
    std::vector<Tag> tags;

    ArchiveItem() = default;
};

} // namespace archive::core
