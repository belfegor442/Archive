#pragma once

#include <string>
#include <vector>

#include "../core/models/Tag.h"
#include "../storage/TagRepository.h"
#include "../storage/ArchiveItemRepository.h"

namespace archive::services {

class TagService {
public:
    TagService(
        storage::TagRepository& tags,
        storage::ArchiveItemRepository& items
    );

    core::Tag create(const std::string& name, const std::string& color = "#6366f1");
    void update(const std::string& id, const std::string& name, const std::string& color);
    void remove(const std::string& id);
    std::vector<core::Tag> get_all();
    std::optional<core::Tag> get_by_id(const std::string& id);
    std::optional<core::Tag> get_by_name(const std::string& name);
    void add_to_item(const std::string& item_id, const std::string& tag_id);
    void remove_from_item(const std::string& item_id, const std::string& tag_id);
    std::vector<core::Tag> get_tags_for_item(const std::string& item_id);
    int get_item_count(const std::string& tag_id);

private:
    storage::TagRepository& tags_;
    storage::ArchiveItemRepository& items_;
};

} // namespace archive::services
