#pragma once

#include <string>
#include <vector>

#include "../core/models/Category.h"
#include "../storage/CategoryRepository.h"
#include "../storage/ArchiveItemRepository.h"
#include "../storage/ActivityRepository.h"

namespace archive::services {

class CategoryService {
public:
    CategoryService(
        storage::CategoryRepository& categories,
        storage::ArchiveItemRepository& items,
        storage::ActivityRepository& activities
    );

    core::Category create(const std::string& name, const std::string& color = "#6366f1",
                          const std::string& description = "");
    void update(const std::string& id, const std::string& name, const std::string& color);
    void remove(const std::string& id);
    std::vector<core::Category> get_all();
    std::optional<core::Category> get_by_id(const std::string& id);
    int get_item_count(const std::string& category_id);

private:
    storage::CategoryRepository& categories_;
    storage::ArchiveItemRepository& items_;
    storage::ActivityRepository& activities_;
};

} // namespace archive::services
