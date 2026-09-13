#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct ShaderWorkspaceConfig {
    std::vector<std::string> favorites;
    std::string lastSelected;
    std::string lastLanguage = "GLSL";

    static ShaderWorkspaceConfig load(const std::filesystem::path& configPath);
    bool save(const std::filesystem::path& configPath) const;
    bool isValid() const;

    static ShaderWorkspaceConfig defaultConfig();

private:
    bool validatePaths() const;
};

}  // namespace monix::renderer_vk
