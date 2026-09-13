#pragma once

#include <string>
#include <cstdint>

namespace archive::hashing {

class FileHasher {
public:
    static std::string hash_file(const std::string& path);
    static std::string hash_buffer(const void* data, size_t size);
    static std::string hash_folder(const std::string& path);
    static bool compare(const std::string& h1, const std::string& h2);
};

} // namespace archive::hashing
