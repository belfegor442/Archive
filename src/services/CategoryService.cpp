#include "CategoryService.h"
#include "../core/utils/Uuid.h"

namespace archive::services {

CategoryService::CategoryService(
    storage::CategoryRepository& categories,
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities
) : categories_(categories)
  , items_(items)
  , activities_(activities)
{}

core::Category CategoryService::create(const std::string& name, const std::string& color,
                                        const std::string& description) {
    core::Category cat;
    cat.id = core::utils::generate_id();
    cat.name = name;
    cat.color = color;
    cat.description = description;
    cat.created_at = core::utils::now_iso();
    categories_.insert(cat);
    return cat;
}

void CategoryService::update(const std::string& id, const std::string& name, const std::string& color) {
    auto cat = categories_.find_by_id(id);
    if (!cat) return;
    cat->name = name;
    cat->color = color;
    categories_.update(*cat);
}

void CategoryService::remove(const std::string& id) {
    categories_.remove(id);
}

std::vector<core::Category> CategoryService::get_all() {
    return categories_.find_all();
}

std::optional<core::Category> CategoryService::get_by_id(const std::string& id) {
    return categories_.find_by_id(id);
}

int CategoryService::get_item_count(const std::string& category_id) {
    auto items = items_.find_by_category(category_id);
    return static_cast<int>(items.size());
}

} // namespace archive::services
