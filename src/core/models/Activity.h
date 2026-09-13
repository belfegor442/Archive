#pragma once

#include <string>

#include "../enums/ActivityAction.h"

namespace archive::core {

struct Activity {
    std::string id;
    std::string item_id;
    ActivityAction action = ActivityAction::Imported;
    std::string details;
    std::string created_at;

    Activity() = default;

    Activity(
        std::string id_,
        std::string item_id_,
        ActivityAction action_,
        std::string details_ = "",
        std::string created_at_ = ""
    ) : id(std::move(id_))
      , item_id(std::move(item_id_))
      , action(action_)
      , details(std::move(details_))
      , created_at(std::move(created_at_))
    {}
};

} // namespace archive::core
