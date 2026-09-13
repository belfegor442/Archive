#include "FileSystem.hpp"

#include <fstream>
#include <sstream>

namespace monix::renderer_vk {

Result<std::string> FileSystem::readText(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return Status::failure("Cannot open file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

Status FileSystem::writeBinary(const std::filesystem::path& path, const std::vector<unsigned char>& data) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return Status::failure("Cannot write file: " + path.string());
    }
    if (!data.empty()) {
        output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    return Status::success();
}

Status FileSystem::writeText(const std::filesystem::path& path, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return Status::failure("Cannot write file: " + path.string());
    }
    output << text;
    return Status::success();
}

std::filesystem::path FileSystem::normalize(const std::filesystem::path& path) {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    if (ec) {
        return path.lexically_normal();
    }
    return absolute.lexically_normal();
}

}  // namespace monix::renderer_vk
