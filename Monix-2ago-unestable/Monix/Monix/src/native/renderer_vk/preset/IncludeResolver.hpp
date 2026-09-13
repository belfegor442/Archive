#pragma once

#include "../core/Result.hpp"

#include <filesystem>
#include <vector>

namespace monix::renderer_vk {

class IncludeResolver {
public:
    void addRoot(std::filesystem::path root);
    void setWorkspaceRoot(std::filesystem::path root);
    Result<std::filesystem::path> resolve(const std::filesystem::path& includingFile,
                                          const std::string& includePath) const;

private:
    std::vector<std::filesystem::path> roots_;
    std::filesystem::path workspaceRoot_;
};

}  // namespace monix::renderer_vk
