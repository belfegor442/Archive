#include "UpdateService.h"

#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace archive::services {

UpdateService::UpdateService(
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities,
    filesystem::StorageManager& storage
) : items_(items)
  , activities_(activities)
  , storage_(storage)
{}

void UpdateService::update_metadata(const std::string& item_id, const std::string& name,
                                     const std::string& description) {
    auto item = items_.find_by_id(item_id);
    if (!item) return;
    item->name = name;
    item->description = description;
    item->last_modified_at = now_iso();
    items_.update(*item);
}

void UpdateService::move_to_trash(const std::string& item_id) {
    items_.update_status(item_id, core::ItemStatus::Deleted);
    core::Activity act;
    act.id = generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::Deleted;
    act.created_at = now_iso();
    activities_.insert(act);
}

void UpdateService::restore_from_trash(const std::string& item_id) {
    items_.update_status(item_id, core::ItemStatus::Archived);
    core::Activity act;
    act.id = generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::Restored;
    act.created_at = now_iso();
    activities_.insert(act);
}

void UpdateService::permanent_delete(const std::string& item_id) {
    storage_.remove_item_dir(item_id);
    items_.remove(item_id);
}

void UpdateService::set_category(const std::string& item_id, const std::string& category_id) {
    items_.update_category(item_id, category_id);
    core::Activity act;
    act.id = generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::CategoryChanged;
    act.created_at = now_iso();
    activities_.insert(act);
}

void UpdateService::toggle_favorite(const std::string& item_id) {
    auto item = items_.find_by_id(item_id);
    if (!item) return;
    items_.set_favorite(item_id, !item->is_favorite);
}

std::string UpdateService::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
    std::ostringstream oss;
    oss << std::hex << dis(gen) << dis(gen);
    std::string id = oss.str();
    id.resize(32);
    return id;
}

std::string UpdateService::now_iso() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace archive::services
