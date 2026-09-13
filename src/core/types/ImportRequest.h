#pragma once

#include <string>
#include <vector>
#include <optional>

namespace archive::core {

struct ImportRequest {
    std::vector<std::string> paths;
    std::optional<std::string> category_id;
    std::vector<std::string> tags;
    std::string description;

    ImportRequest() = default;
};

} // namespace archive::core
