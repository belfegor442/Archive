#pragma once

#include <string>
#include "../enums/UndoStatus.h"

namespace archive::core {

struct UndoRecord {
    std::string id;
    std::string operation_id;
    int moves_count = 0;
    std::string root_path;
    UndoStatus status = UndoStatus::Available;
    std::string created_at;
};

} // namespace archive::core
