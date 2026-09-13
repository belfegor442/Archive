#include "PresetValidator.hpp"

#include <filesystem>
#include <unordered_set>

namespace monix::renderer_vk {

Diagnostics PresetValidator::validate(const PresetIr& preset) const {
    Diagnostics diagnostics;
    if (preset.passes.empty()) {
        diagnostics.error("Preset has no passes");
    }

    std::unordered_set<std::string> aliases;
    for (const auto& pass : preset.passes) {
        std::error_code ec;
        if (!std::filesystem::exists(pass.shaderPath, ec)) {
            diagnostics.error("Shader file not found: " + pass.shaderPath.string());
        }
        if (!pass.alias.empty() && !aliases.insert(pass.alias).second) {
            diagnostics.error("Duplicate pass alias: " + pass.alias);
        }
        if (pass.floatFramebuffer && pass.srgbFramebuffer) {
            diagnostics.warning("Pass " + std::to_string(pass.index) + " requests both float and sRGB framebuffer");
        }
    }

    for (const auto& texture : preset.externalTextures) {
        if (texture.alias.empty()) {
            diagnostics.warning("External texture " + std::to_string(texture.textureIndex) + " has no alias");
        } else if (!aliases.insert(texture.alias).second) {
            diagnostics.error("Duplicate alias across pass/texture: " + texture.alias);
        }
        std::error_code ec;
        if (!std::filesystem::exists(texture.path, ec)) {
            diagnostics.warning("External texture file not found; runtime will bind placeholder: " + texture.path.string());
        }
    }
    return diagnostics;
}

}  // namespace monix::renderer_vk
