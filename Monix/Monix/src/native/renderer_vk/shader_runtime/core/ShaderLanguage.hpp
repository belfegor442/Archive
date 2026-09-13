#pragma once

#include <cstdint>
#include <string>

namespace monix::renderer_vk {

enum class ShaderLanguage : std::uint8_t {
    Unknown = 0,
    Slang,
    GLSL,
    CG
};

inline const char* shaderLanguageName(ShaderLanguage lang) {
    switch (lang) {
    case ShaderLanguage::Slang:  return "Slang";
    case ShaderLanguage::GLSL:   return "GLSL";
    case ShaderLanguage::CG:     return "CG";
    case ShaderLanguage::Unknown:return "Unknown";
    }
    return "Unknown";
}

inline ShaderLanguage detectLanguageFromExtension(const std::string& ext) {
    if (ext == ".slang" || ext == ".slangp") return ShaderLanguage::Slang;
    if (ext == ".glslp") return ShaderLanguage::GLSL;
    if (ext == ".glsl" || ext == ".vert" || ext == ".frag" ||
        ext == ".geom" || ext == ".comp")   return ShaderLanguage::GLSL;
    if (ext == ".cg")   return ShaderLanguage::CG;
    return ShaderLanguage::Unknown;
}

}  // namespace monix::renderer_vk
