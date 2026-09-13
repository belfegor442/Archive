#pragma once

#include "../core/Diagnostics.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::renderer_vk {

enum class ScaleType {
    Source,
    Viewport,
    Absolute,
    Original
};

enum class WrapMode {
    ClampToBorder,
    ClampToEdge,
    Repeat,
    MirroredRepeat
};

struct PresetAstEntry {
    std::string key;
    std::string value;
    int line = 0;
};

struct PresetAst {
    std::filesystem::path path;
    std::filesystem::path baseDirectory;
    std::vector<PresetAstEntry> entries;
    Diagnostics diagnostics;

    std::optional<std::string> get(std::string_view key) const;
    std::vector<PresetAstEntry> findPrefix(std::string_view prefix) const;
};

struct ScaleRule {
    ScaleType typeX = ScaleType::Source;
    ScaleType typeY = ScaleType::Source;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

struct PresetPassIr {
    int index = 0;
    std::filesystem::path shaderPath;
    std::string shaderPathRaw;
    std::string alias;
    bool filterLinear = false;
    WrapMode wrapMode = WrapMode::ClampToBorder;
    bool mipmapInput = false;
    bool floatFramebuffer = false;
    bool srgbFramebuffer = false;
    bool framebufferFeedback = false;
    ScaleRule scale;
};

struct PresetTextureIr {
    int textureIndex = 0;
    int aliasIndex = 0;
    std::filesystem::path path;
    std::string pathRaw;
    std::string alias;
    bool filterLinear = true;
    WrapMode wrapMode = WrapMode::ClampToBorder;
    bool mipmap = false;
};

struct PresetParameterOverride {
    std::string name;
    std::string rawValue;
};

struct PresetIr {
    std::filesystem::path path;
    std::filesystem::path baseDirectory;
    std::string name;
    std::vector<PresetPassIr> passes;
    std::vector<PresetTextureIr> externalTextures;
    std::vector<PresetParameterOverride> parameterOverrides;
    Diagnostics diagnostics;
};

const char* toString(ScaleType type);
const char* toString(WrapMode mode);
ScaleType parseScaleType(std::string_view text);
WrapMode parseWrapMode(std::string_view text);
bool parseBool(std::string_view text, bool defaultValue);
float parseFloat(std::string_view text, float defaultValue);

}  // namespace monix::renderer_vk
