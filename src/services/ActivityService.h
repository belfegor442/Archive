#pragma once

#include <string>
#include <vector>

#include "../core/models/Activity.h"
#include "../storage/ActivityRepository.h"

namespace archive::services {

class ActivityService {
public:
    explicit ActivityService(storage::ActivityRepository& activities);

    std::vector<core::Activity> get_recent(int limit = 50);
    std::vector<core::Activity> get_for_item(const std::string& item_id);
    void log(const std::string& item_id, core::ActivityAction action, const std::string& details = "");

private:
    storage::ActivityRepository& activities_;
    std::string generate_id();
    std::string now_iso();
};

} // namespace archive::services
