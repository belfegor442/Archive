#pragma once

#include <filesystem>
#include "../../renderer_vk/opengl/GlBackend.hpp"

namespace monix {

renderer_vk::GlPresetConfig parseGlslpPreset(
    const std::filesystem::path& glslpPath,
    const std::filesystem::path& rootDir);

} // namespace monix
