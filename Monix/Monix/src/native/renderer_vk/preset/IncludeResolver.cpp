#include "IncludeResolver.hpp"

#include "../core/FileSystem.hpp"

#include <filesystem>

namespace monix::renderer_vk {

void IncludeResolver::addRoot(std::filesystem::path root) {
    roots_.push_back(FileSystem::normalize(root));
}

void IncludeResolver::setWorkspaceRoot(std::filesystem::path root) {
    workspaceRoot_ = FileSystem::normalize(root);
}

Result<std::filesystem::path> IncludeResolver::resolve(const std::filesystem::path& includingFile,
                                                       const std::string& includePath) const {
    std::vector<std::filesystem::path> candidates;
    if (!includingFile.empty()) {
        candidates.push_back(includingFile.parent_path() / includePath);
    }
    for (const auto& root : roots_) {
        candidates.push_back(root / includePath);
    }

    for (const auto& candidate : candidates) {
        std::error_code ec;
        auto normalized = FileSystem::normalize(candidate);
        if (!workspaceRoot_.empty()) {
            auto rel = std::filesystem::relative(normalized, workspaceRoot_, ec);
            if (ec || rel.string().find("..") == 0) {
                continue;
            }
        }
        if (std::filesystem::exists(normalized, ec) && !ec) {
            return normalized;
        }
    }
    return Status::failure("Include not found: " + includePath + " from " + includingFile.string());
}

}  // namespace monix::renderer_vk
