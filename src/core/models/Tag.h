#pragma once

#include <string>

namespace archive::core {

struct Tag {
    std::string id;
    std::string name;
    std::string color;

    Tag() = default;

    Tag(std::string id_, std::string name_, std::string color_ = "#6366f1")
        : id(std::move(id_))
        , name(std::move(name_))
        , color(std::move(color_))
    {}
};

} // namespace archive::core
