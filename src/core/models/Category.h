#pragma once

#include <string>
#include <optional>

namespace archive::core {

struct Category {
    std::string id;
    std::string name;
    std::string description;
    std::string color;
    std::string icon;
    std::optional<std::string> parent_id;
    std::string created_at;

    Category() = default;

    Category(
        std::string id_,
        std::string name_,
        std::string description_ = "",
        std::string color_ = "#6366f1",
        std::string icon_ = "",
        std::optional<std::string> parent_id_ = std::nullopt,
        std::string created_at_ = ""
    ) : id(std::move(id_))
      , name(std::move(name_))
      , description(std::move(description_))
      , color(std::move(color_))
      , icon(std::move(icon_))
      , parent_id(std::move(parent_id_))
      , created_at(std::move(created_at_))
    {}
};

} // namespace archive::core
