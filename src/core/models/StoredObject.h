#pragma once

#include <string>
#include <cstdint>

namespace archive::core {

struct StoredObject {
    std::string id;
    std::string item_id;
    std::string version_id;
    std::string storage_path;
    uint64_t size = 0;
    std::string checksum;
    std::string created_at;

    StoredObject() = default;

    StoredObject(
        std::string id_,
        std::string item_id_,
        std::string version_id_,
        std::string storage_path_,
        uint64_t size_,
        std::string checksum_,
        std::string created_at_
    ) : id(std::move(id_))
      , item_id(std::move(item_id_))
      , version_id(std::move(version_id_))
      , storage_path(std::move(storage_path_))
      , size(size_)
      , checksum(std::move(checksum_))
      , created_at(std::move(created_at_))
    {}
};

} // namespace archive::core
