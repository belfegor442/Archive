#pragma once

#include <string>

namespace archive::core {

struct ClassificationRule {
    std::string id;
    std::string name;
    std::string pattern;
    std::string target_path;
    int priority = 0;
    bool enabled = true;
    std::string created_at;

    ClassificationRule() = default;
};

} // namespace archive::core
