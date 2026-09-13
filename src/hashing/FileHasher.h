#pragma once

#include <string>
#include <cstdint>

namespace archive::hashing {

class FileHasher {
public:
    static std::string hash_file(const std::string& path);
    static std::string hash_buffer(const void* data, size_t size);
};

} // namespace archive::hashing
