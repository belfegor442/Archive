#pragma once

#include <string>

namespace archive::core {

struct MoveDetail {
    std::string source;
    std::string destination;
    double confidence = 0.0;
    std::string reason;
    std::string category;
};

} // namespace archive::core
