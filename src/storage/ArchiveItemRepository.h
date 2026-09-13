#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/ArchiveItem.h"
#include "../core/types/DashboardStats.h"
#include "DatabaseManager.h"

namespace archive::storage {

class ArchiveItemRepository {
public:
    explicit ArchiveItemRepository(DatabaseManager& db);

    std::string insert(const core::ArchiveItem& item);
    void update(const core::ArchiveItem& item);
    void remove(const std::string& id);
    std::optional<core::ArchiveItem> find_by_id(const std::string& id);
    std::vector<core::ArchiveItem> find_all();
    std::vector<core::ArchiveItem> find_by_status(core::ItemStatus status);
    std::vector<core::ArchiveItem> find_by_category(const std::string& category_id);
    std::vector<core::ArchiveItem> find_favorites();
    std::vector<core::ArchiveItem> search(const std::string& query);
    int count();
    int count_by_status(core::ItemStatus status);
    void update_status(const std::string& id, core::ItemStatus status);
    void set_favorite(const std::string& id, bool favorite);
    void update_category(const std::string& id, const std::string& category_id);
    void increment_version(const std::string& id);

    std::vector<core::Tag> get_tags(const std::string& item_id);
    void add_tag(const std::string& item_id, const std::string& tag_id);
    void remove_tag(const std::string& item_id, const std::string& tag_id);

    core::DashboardStats get_stats();

private:
    DatabaseManager& db_;
    core::ArchiveItem read_item(DatabaseManager::Statement& stmt);
    void load_tags(core::ArchiveItem& item);
};

} // namespace archive::storage
