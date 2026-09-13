#include "TagService.h"
#include "../core/utils/Uuid.h"

namespace archive::services {

TagService::TagService(
    storage::TagRepository& tags,
    storage::ArchiveItemRepository& items
) : tags_(tags)
  , items_(items)
{}

core::Tag TagService::create(const std::string& name, const std::string& color) {
    core::Tag tag;
    tag.id = core::utils::generate_id();
    tag.name = name;
    tag.color = color;
    tags_.insert(tag);
    return tag;
}

void TagService::update(const std::string& id, const std::string& name, const std::string& color) {
    auto tag = tags_.find_by_id(id);
    if (!tag) return;
    tag->name = name;
    tag->color = color;
    tags_.update(*tag);
}

void TagService::remove(const std::string& id) {
    tags_.remove(id);
}

std::vector<core::Tag> TagService::get_all() {
    return tags_.find_all();
}

std::optional<core::Tag> TagService::get_by_id(const std::string& id) {
    return tags_.find_by_id(id);
}

std::optional<core::Tag> TagService::get_by_name(const std::string& name) {
    return tags_.find_by_name(name);
}

void TagService::add_to_item(const std::string& item_id, const std::string& tag_id) {
    items_.add_tag(item_id, tag_id);
}

void TagService::remove_from_item(const std::string& item_id, const std::string& tag_id) {
    items_.remove_tag(item_id, tag_id);
}

std::vector<core::Tag> TagService::get_tags_for_item(const std::string& item_id) {
    return items_.get_tags(item_id);
}

int TagService::get_item_count(const std::string& tag_id) {
    auto all_items = items_.find_all();
    int count = 0;
    for (const auto& item : all_items) {
        for (const auto& tag : item.tags) {
            if (tag.id == tag_id) {
                count++;
                break;
            }
        }
    }
    return count;
}

} // namespace archive::services
