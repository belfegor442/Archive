#pragma once

#include <filesystem>
#include <string>

namespace monix::renderer_vk {

class ShaderWorkspace {
public:
    ShaderWorkspace() = default;
    explicit ShaderWorkspace(std::filesystem::path root);

    bool initialize();

    std::filesystem::path root() const { return root_; }

    std::filesystem::path glslRoot() const;
    std::filesystem::path slangRoot() const;
    std::filesystem::path slangpRoot() const;

    std::filesystem::path cacheRoot() const;
    std::filesystem::path configFile() const;

    bool ensureDirectories();

    bool isValid() const;

    std::string workspaceName() const;

    static ShaderWorkspace resolveFromExecutable();

private:
    std::filesystem::path root_;
    bool valid_ = false;
};

}  // namespace monix::renderer_vk
