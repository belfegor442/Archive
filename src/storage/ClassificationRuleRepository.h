#pragma once

#include <string>
#include <vector>
#include <optional>

#include "../core/models/ClassificationRule.h"
#include "DatabaseManager.h"

namespace archive::storage {

class ClassificationRuleRepository {
public:
    explicit ClassificationRuleRepository(DatabaseManager& db);

    void insert(const core::ClassificationRule& rule);
    void update(const core::ClassificationRule& rule);
    void remove(const std::string& id);
    std::optional<core::ClassificationRule> find_by_id(const std::string& id);
    std::vector<core::ClassificationRule> find_all();
    std::vector<core::ClassificationRule> find_enabled();

private:
    DatabaseManager& db_;
    core::ClassificationRule read_rule(DatabaseManager::Statement& stmt);
};

} // namespace archive::storage
