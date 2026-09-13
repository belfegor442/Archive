#pragma once

#include <string>
#include <stdexcept>

namespace archive::core {

enum class ItemType {
    File,
    Folder,
    Project,
    Document
};

inline std::string to_string(ItemType type) {
    switch (type) {
        case ItemType::File:      return "file";
        case ItemType::Folder:    return "folder";
        case ItemType::Project:   return "project";
        case ItemType::Document:  return "document";
    }
    throw std::invalid_argument("Unknown ItemType");
}

inline ItemType item_type_from_string(const std::string& s) {
    if (s == "file")      return ItemType::File;
    if (s == "folder")    return ItemType::Folder;
    if (s == "project")   return ItemType::Project;
    if (s == "document")  return ItemType::Document;
    throw std::invalid_argument("Unknown ItemType: " + s);
}

} // namespace archive::core
