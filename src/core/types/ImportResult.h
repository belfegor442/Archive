#pragma once

#include <string>
#include <vector>

#include "../models/ArchiveItem.h"

namespace archive::core {

struct ImportError {
    std::string path;
    std::string error;
};

struct ImportResult {
    std::vector<ArchiveItem> items;
    std::vector<ImportError> errors;

    ImportResult() = default;

    int success_count() const { return static_cast<int>(items.size()); }
    int error_count() const { return static_cast<int>(errors.size()); }
    bool has_errors() const { return !errors.empty(); }
};

} // namespace archive::core
