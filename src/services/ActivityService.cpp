#include "ActivityService.h"

#include <random>
#include <sstream>
#include <chrono>
#include <iomanip>

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
    act.id = generate_id();
    act.item_id = item_id;
    act.action = action;
    act.details = details;
    act.created_at = now_iso();
    activities_.insert(act);
}

std::string ActivityService::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, std::numeric_limits<uint64_t>::max());
    std::ostringstream oss;
    oss << std::hex << dis(gen) << dis(gen);
    std::string id = oss.str();
    id.resize(32);
    return id;
}

std::string ActivityService::now_iso() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace archive::services
