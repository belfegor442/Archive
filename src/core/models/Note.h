#pragma once

#include <string>

namespace archive::core {

struct Note {
    std::string id;
    std::string item_id;
    std::string content;
    std::string created_at;
    std::string updated_at;

    Note() = default;

    Note(
        std::string id_,
        std::string item_id_,
        std::string content_,
        std::string created_at_ = "",
        std::string updated_at_ = ""
    ) : id(std::move(id_))
      , item_id(std::move(item_id_))
      , content(std::move(content_))
      , created_at(std::move(created_at_))
      , updated_at(std::move(updated_at_))
    {}
};

} // namespace archive::core
