#include "ActivityRepository.h"

#include "../core/enums/ActivityAction.h"

namespace archive::storage {

ActivityRepository::ActivityRepository(DatabaseManager& db)
    : db_(db)
{}

std::string ActivityRepository::insert(const core::Activity& activity) {
    auto stmt = db_.prepare(R"(
        INSERT INTO activity_log (id, item_id, action, details, created_at)
        VALUES (?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, activity.id);
    stmt.bind_text(2, activity.item_id);
    stmt.bind_text(3, core::to_string(activity.action));
    stmt.bind_text(4, activity.details);
    stmt.bind_text(5, activity.created_at);
    stmt.step();
    return activity.id;
}

std::vector<core::Activity> ActivityRepository::find_by_item(const std::string& item_id) {
    std::vector<core::Activity> activities;
    auto stmt = db_.prepare("SELECT * FROM activity_log WHERE item_id=? ORDER BY created_at DESC");
    stmt.bind_text(1, item_id);
    while (stmt.step()) {
        activities.push_back(read_activity(stmt));
    }
    return activities;
}

std::vector<core::Activity> ActivityRepository::find_recent(int limit) {
    std::vector<core::Activity> activities;
    auto stmt = db_.prepare("SELECT * FROM activity_log ORDER BY created_at DESC LIMIT ?");
    stmt.bind_int(1, limit);
    while (stmt.step()) {
        activities.push_back(read_activity(stmt));
    }
    return activities;
}

void ActivityRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM activity_log WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step();
}

core::Activity ActivityRepository::read_activity(DatabaseManager::Statement& stmt) {
    core::Activity act;
    act.id = stmt.column_text(0);
    act.item_id = stmt.column_text(1);
    act.action = core::activity_action_from_string(stmt.column_text(2));
    act.details = stmt.column_text(3);
    act.created_at = stmt.column_text(4);
    return act;
}

} // namespace archive::storage
