#pragma once

#include <string>
#include <vector>

namespace archive::core {

struct Classification {
    std::string id;
    std::string scan_item_id;
    std::string taxonomy_path;
    double confidence = 0.0;
    std::string reason;

    Classification() = default;
};

struct ClassificationGroup {
    std::string id;
    std::string classification_id;
    std::string group_member_id;

    ClassificationGroup() = default;
};

} // namespace archive::core
