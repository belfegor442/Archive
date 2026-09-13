#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class ItemStatus {
    Archived,
    Deleted
};

inline std::string to_string(ItemStatus status) {
    switch (status) {
        case ItemStatus::Archived: return "archived";
        case ItemStatus::Deleted:  return "deleted";
    }
    throw std::invalid_argument("Unknown ItemStatus");
}

inline ItemStatus item_status_from_string(const std::string& s) {
    if (s == "archived") return ItemStatus::Archived;
    if (s == "deleted")  return ItemStatus::Deleted;
    throw std::invalid_argument("Unknown ItemStatus: " + s);
}

} // namespace archive::core
