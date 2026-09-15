#pragma once

#include <string>

namespace archive::core {

struct UndoEntry {
    std::string undo_id;
    std::string source_path;
    std::string dest_path;
    int move_index = 0;
};

} // namespace archive::core
