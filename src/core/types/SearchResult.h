#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "../models/ArchiveItem.h"

namespace archive::core {

struct SearchResult {
    std::vector<ArchiveItem> items;
    int total = 0;
    std::string query;

    SearchResult() = default;

    SearchResult(std::vector<ArchiveItem> items_, int total_, std::string query_)
        : items(std::move(items_))
        , total(total_)
        , query(std::move(query_))
    {}
};

} // namespace archive::core
