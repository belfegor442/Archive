#include "UpdateService.h"
#include "../core/utils/Uuid.h"
#include "../storage/Transaction.h"

namespace archive::services {

UpdateService::UpdateService(
    storage::DatabaseManager& db,
    storage::ArchiveItemRepository& items,
    storage::ActivityRepository& activities,
    filesystem::StorageManager& storage
) : db_(db)
  , items_(items)
  , activities_(activities)
  , storage_(storage)
{}

void UpdateService::update_metadata(const std::string& item_id, const std::string& name,
                                     const std::string& description) {
    auto item = items_.find_by_id(item_id);
    if (!item) return;
    item->name = name;
    item->description = description;
    item->last_modified_at = core::utils::now_iso();
    items_.update(*item);
}

void UpdateService::move_to_trash(const std::string& item_id) {
    items_.update_status(item_id, core::ItemStatus::Deleted);
    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::Deleted;
    act.created_at = core::utils::now_iso();
    activities_.insert(act);
}

void UpdateService::restore_from_trash(const std::string& item_id) {
    items_.update_status(item_id, core::ItemStatus::Archived);
    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::Restored;
    act.created_at = core::utils::now_iso();
    activities_.insert(act);
}

void UpdateService::permanent_delete(const std::string& item_id) {
    auto item = items_.find_by_id(item_id);
    if (!item) return;

    {
        storage::Transaction tx(db_);

        core::Activity act;
        act.id = core::utils::generate_id();
        act.item_id = item_id;
        act.action = core::ActivityAction::Deleted;
        act.created_at = core::utils::now_iso();
        activities_.insert(act);

        items_.remove(item_id);
        tx.commit();
    }

    storage_.remove_item_dir(item_id);
}

void UpdateService::set_category(const std::string& item_id, const std::string& category_id) {
    items_.update_category(item_id, category_id);
    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = core::ActivityAction::CategoryChanged;
    act.created_at = core::utils::now_iso();
    activities_.insert(act);
}

void UpdateService::toggle_favorite(const std::string& item_id) {
    auto stmt = db_.prepare(
        "UPDATE archive_items SET is_favorite = 1 - is_favorite WHERE id = ?");
    stmt.bind_text(1, item_id);
    stmt.step_done();
}

} // namespace archive::services
