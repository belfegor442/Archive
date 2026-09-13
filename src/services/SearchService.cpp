#include "SearchService.h"

namespace archive::services {

SearchService::SearchService(storage::ArchiveItemRepository& items)
    : items_(items)
{}

core::SearchResult SearchService::search(const std::string& query, const SearchFilters& filters) {
    std::vector<core::ArchiveItem> results;

    if (query.empty() && !filters.category_id && !filters.type && !filters.is_favorite && !filters.tag) {
        results = items_.find_all();
    } else if (!query.empty()) {
        results = items_.search(query);
    } else {
        results = items_.find_all();
    }

    std::vector<core::ArchiveItem> filtered;
    for (auto& item : results) {
        bool pass = true;

        if (filters.category_id && item.category_id != filters.category_id) {
            pass = false;
        }
        if (filters.type && core::to_string(item.type) != filters.type) {
            pass = false;
        }
        if (filters.is_favorite && item.is_favorite != *filters.is_favorite) {
            pass = false;
        }

        if (pass) {
            filtered.push_back(std::move(item));
        }
    }

    int total = static_cast<int>(filtered.size());
    return core::SearchResult(std::move(filtered), total, query);
}

std::vector<core::ArchiveItem> SearchService::find_by_status(core::ItemStatus status) {
    return items_.find_by_status(status);
}

std::vector<core::ArchiveItem> SearchService::find_favorites() {
    return items_.find_favorites();
}

std::vector<core::ArchiveItem> SearchService::find_recent(int limit) {
    auto all = items_.find_all();
    if (static_cast<int>(all.size()) > limit) {
        all.resize(static_cast<size_t>(limit));
    }
    return all;
}

std::vector<core::ArchiveItem> SearchService::find_by_category(const std::string& category_id) {
    return items_.find_by_category(category_id);
}

} // namespace archive::services
