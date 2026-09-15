#include "ClassificationRuleRepository.h"

namespace archive::storage {

ClassificationRuleRepository::ClassificationRuleRepository(DatabaseManager& db)
    : db_(db)
{}

void ClassificationRuleRepository::insert(const core::ClassificationRule& rule) {
    auto stmt = db_.prepare(R"(
        INSERT INTO classification_rules (id, name, pattern, target_path, priority, enabled, created_at)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    stmt.bind_text(1, rule.id);
    stmt.bind_text(2, rule.name);
    stmt.bind_text(3, rule.pattern);
    stmt.bind_text(4, rule.target_path);
    stmt.bind_int(5, rule.priority);
    stmt.bind_int(6, rule.enabled ? 1 : 0);
    stmt.bind_text(7, rule.created_at);
    stmt.step_done();
}

void ClassificationRuleRepository::update(const core::ClassificationRule& rule) {
    auto stmt = db_.prepare(R"(
        UPDATE classification_rules SET name=?, pattern=?, target_path=?,
            priority=?, enabled=?
        WHERE id=?
    )");
    stmt.bind_text(1, rule.name);
    stmt.bind_text(2, rule.pattern);
    stmt.bind_text(3, rule.target_path);
    stmt.bind_int(4, rule.priority);
    stmt.bind_int(5, rule.enabled ? 1 : 0);
    stmt.bind_text(6, rule.id);
    stmt.step_done();
}

void ClassificationRuleRepository::remove(const std::string& id) {
    auto stmt = db_.prepare("DELETE FROM classification_rules WHERE id=?");
    stmt.bind_text(1, id);
    stmt.step_done();
}

std::optional<core::ClassificationRule> ClassificationRuleRepository::find_by_id(const std::string& id) {
    auto stmt = db_.prepare(R"(
        SELECT id, name, pattern, target_path, priority, enabled, created_at
        FROM classification_rules WHERE id=?
    )");
    stmt.bind_text(1, id);
    if (stmt.step()) return read_rule(stmt);
    return std::nullopt;
}

std::vector<core::ClassificationRule> ClassificationRuleRepository::find_all() {
    std::vector<core::ClassificationRule> rules;
    auto stmt = db_.prepare(R"(
        SELECT id, name, pattern, target_path, priority, enabled, created_at
        FROM classification_rules ORDER BY priority DESC
    )");
    while (stmt.step()) {
        rules.push_back(read_rule(stmt));
    }
    return rules;
}

std::vector<core::ClassificationRule> ClassificationRuleRepository::find_enabled() {
    std::vector<core::ClassificationRule> rules;
    auto stmt = db_.prepare(R"(
        SELECT id, name, pattern, target_path, priority, enabled, created_at
        FROM classification_rules WHERE enabled=1 ORDER BY priority DESC
    )");
    while (stmt.step()) {
        rules.push_back(read_rule(stmt));
    }
    return rules;
}

core::ClassificationRule ClassificationRuleRepository::read_rule(DatabaseManager::Statement& stmt) {
    core::ClassificationRule rule;
    rule.id = stmt.column_text(0);
    rule.name = stmt.column_text(1);
    rule.pattern = stmt.column_text(2);
    rule.target_path = stmt.column_text(3);
    rule.priority = stmt.column_int(4);
    rule.enabled = stmt.column_int(5) != 0;
    rule.created_at = stmt.column_text(6);
    return rule;
}

} // namespace archive::storage
