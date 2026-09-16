#include "SearchService.h"

#include <algorithm>
#include <cctype>

namespace archive::services {

SearchService::SearchService(storage::ArchiveItemRepository& items)
    : items_(items)
{}

core::SearchResult SearchService::search(const std::string& query, const SearchFilters& filters) {
    std::vector<core::ArchiveItem> results;

    bool has_query = !query.empty();

    if (has_query) {
        results = items_.search(query);
    } else if (filters.is_favorite.has_value() && *filters.is_favorite) {
        results = items_.find_favorites();
    } else if (filters.type.has_value()) {
        auto status = core::ItemStatus::Archived;
        results = items_.find_by_status(status);
    } else {
        results = items_.find_all();
    }

    if (filters.is_favorite.has_value()) {
        bool want_fav = *filters.is_favorite;
        auto it = std::remove_if(results.begin(), results.end(),
            [want_fav](const core::ArchiveItem& i) { return i.is_favorite != want_fav; });
        results.erase(it, results.end());
    }

    if (filters.type.has_value()) {
        auto it = std::remove_if(results.begin(), results.end(),
            [&](const core::ArchiveItem& i) { return core::to_string(i.type) != filters.type; });
        results.erase(it, results.end());
    }

    if (filters.category_id.has_value()) {
        auto it = std::remove_if(results.begin(), results.end(),
            [&](const core::ArchiveItem& i) { return i.category_id != filters.category_id; });
        results.erase(it, results.end());
    }

    int total = static_cast<int>(results.size());
    return core::SearchResult(std::move(results), total, query);
}

std::vector<core::ArchiveItem> SearchService::find_by_status(core::ItemStatus status) {
    return items_.find_by_status(status);
}

std::vector<core::ArchiveItem> SearchService::find_favorites() {
    return items_.find_favorites();
}

std::vector<core::ArchiveItem> SearchService::find_recent(int limit) {
    return items_.find_recent(limit);
}

std::vector<core::ArchiveItem> SearchService::find_by_category(const std::string& category_id) {
    return items_.find_by_category(category_id);
}

} // namespace archive::services
