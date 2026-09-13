#include "ActivityService.h"
#include "../core/utils/Uuid.h"

namespace archive::services {

ActivityService::ActivityService(storage::ActivityRepository& activities)
    : activities_(activities)
{}

std::vector<core::Activity> ActivityService::get_recent(int limit) {
    return activities_.find_recent(limit);
}

std::vector<core::Activity> ActivityService::get_for_item(const std::string& item_id) {
    return activities_.find_by_item(item_id);
}

void ActivityService::log(const std::string& item_id, core::ActivityAction action, const std::string& details) {
    core::Activity act;
    act.id = core::utils::generate_id();
    act.item_id = item_id;
    act.action = action;
    act.details = details;
    act.created_at = core::utils::now_iso();
    activities_.insert(act);
}

} // namespace archive::services
