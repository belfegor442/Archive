#pragma once

#include "Result.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace monix::renderer_vk {

class FileSystem {
public:
    static Result<std::string> readText(const std::filesystem::path& path);
    static Status writeBinary(const std::filesystem::path& path, const std::vector<unsigned char>& data);
    static Status writeText(const std::filesystem::path& path, const std::string& text);
    static std::filesystem::path normalize(const std::filesystem::path& path);
};

}  // namespace monix::renderer_vk
