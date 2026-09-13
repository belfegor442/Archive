#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "../models/ArchiveItem.h"

namespace archive::core {

struct CategoryCount {
    std::string category_id;
    std::string name;
    int count = 0;
    std::string color;
};

struct TypeCount {
    std::string type;
    int count = 0;
};

struct DashboardStats {
    int total_items = 0;
    int archived_items = 0;
    int favorite_items = 0;
    int deleted_items = 0;
    uint64_t total_size = 0;
    std::vector<ArchiveItem> recent_items;
    std::vector<CategoryCount> category_counts;
    std::vector<TypeCount> type_counts;

    DashboardStats() = default;
};

} // namespace archive::core
