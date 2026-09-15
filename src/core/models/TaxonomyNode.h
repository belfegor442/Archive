#pragma once

#include <string>
#include <vector>

namespace archive::core {

struct TaxonomyNode {
    std::string id;
    std::string name;
    std::string parent_id;
    int level = 0;
    int file_count = 0;
    std::string icon;

    TaxonomyNode() = default;
};

} // namespace archive::core
