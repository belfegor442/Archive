#include "ShaderWorkspace.hpp"

#include <cstdio>

namespace monix::renderer_vk {

ShaderWorkspace::ShaderWorkspace(std::filesystem::path root)
    : root_(std::move(root)) {}

bool ShaderWorkspace::initialize() {
    if (root_.empty()) {
        *this = resolveFromExecutable();
    }

    std::error_code ec;
    root_ = std::filesystem::weakly_canonical(root_, ec);
    if (ec) {
        root_ = root_.lexically_normal();
    }

    valid_ = ensureDirectories();
    return valid_;
}

std::filesystem::path ShaderWorkspace::glslRoot() const {
    return root_ / "GLSL";
}

std::filesystem::path ShaderWorkspace::slangRoot() const {
    return root_ / "SLANG";
}

std::filesystem::path ShaderWorkspace::slangpRoot() const {
    return root_ / "SLANGP";
}

std::filesystem::path ShaderWorkspace::cacheRoot() const {
    return root_ / ".cache";
}

std::filesystem::path ShaderWorkspace::configFile() const {
    return root_ / "config.json";
}

bool ShaderWorkspace::ensureDirectories() {
    std::error_code ec;

    std::filesystem::create_directories(root_, ec);
    if (ec) return false;

    std::filesystem::create_directories(glslRoot(), ec);
    if (ec) return false;

    std::filesystem::create_directories(slangRoot(), ec);
    if (ec) return false;

    std::filesystem::create_directories(slangpRoot(), ec);
    if (ec) return false;

    std::filesystem::create_directories(cacheRoot(), ec);
    if (ec) return false;

    return true;
}

bool ShaderWorkspace::isValid() const {
    return valid_ && !root_.empty() && std::filesystem::exists(root_);
}

std::string ShaderWorkspace::workspaceName() const {
    return root_.filename().string();
}

ShaderWorkspace ShaderWorkspace::resolveFromExecutable() {
    std::error_code ec;
    auto exePath = std::filesystem::current_path(ec);
    if (ec) {
        exePath = std::filesystem::path(".");
    }

    auto candidate = exePath / "Shaders";
    if (std::filesystem::exists(candidate)) {
        return ShaderWorkspace(candidate);
    }

    candidate = exePath.parent_path() / "Shaders";
    if (std::filesystem::exists(candidate)) {
        return ShaderWorkspace(candidate);
    }

    return ShaderWorkspace(exePath / "Shaders");
}

}  // namespace monix::renderer_vk
