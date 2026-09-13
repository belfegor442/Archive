#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/ArchiveItem.h"
#include "../core/types/SearchResult.h"
#include "../storage/ArchiveItemRepository.h"

namespace archive::services {

struct SearchFilters {
    std::optional<std::string> category_id;
    std::optional<std::string> type;
    std::optional<bool> is_favorite;
    std::optional<std::string> tag;
};

class SearchService {
public:
    explicit SearchService(storage::ArchiveItemRepository& items);

    core::SearchResult search(const std::string& query, const SearchFilters& filters = {});
    std::vector<core::ArchiveItem> find_by_status(core::ItemStatus status);
    std::vector<core::ArchiveItem> find_favorites();
    std::vector<core::ArchiveItem> find_recent(int limit = 10);
    std::vector<core::ArchiveItem> find_by_category(const std::string& category_id);

private:
    storage::ArchiveItemRepository& items_;
};

} // namespace archive::services
