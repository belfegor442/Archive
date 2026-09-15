#pragma once

#include <string>
#include "../enums/ScanStatus.h"

namespace archive::core {

struct Scan {
    std::string id;
    std::string root_path;
    ScanStatus status = ScanStatus::Pending;
    int file_count = 0;
    int folder_count = 0;
    std::string started_at;
    std::string completed_at;

    Scan() = default;
};

} // namespace archive::core
