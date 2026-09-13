#pragma once

#include <string>
#include <cstdint>

namespace archive::core {

struct Version {
    std::string id;
    std::string item_id;
    int version_number = 0;
    std::string storage_path;
    std::string checksum;
    uint64_t size = 0;
    std::string notes;
    std::string created_at;

    Version() = default;

    Version(
        std::string id_,
        std::string item_id_,
        int version_number_,
        std::string storage_path_,
        std::string checksum_ = "",
        uint64_t size_ = 0,
        std::string notes_ = "",
        std::string created_at_ = ""
    ) : id(std::move(id_))
      , item_id(std::move(item_id_))
      , version_number(version_number_)
      , storage_path(std::move(storage_path_))
      , checksum(std::move(checksum_))
      , size(size_)
      , notes(std::move(notes_))
      , created_at(std::move(created_at_))
    {}
};

} // namespace archive::core
