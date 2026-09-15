#pragma once

#include <string>
#include "../enums/MoveStatus.h"

namespace archive::core {

struct OrgMove {
    std::string id;
    std::string plan_id;
    std::string scan_item_id;
    std::string source_path;
    std::string dest_path;
    double confidence = 0.0;
    std::string reason;
    MoveStatus status = MoveStatus::Planned;

    OrgMove() = default;
};

} // namespace archive::core
