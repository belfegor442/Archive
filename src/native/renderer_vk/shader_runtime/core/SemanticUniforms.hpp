#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace monix::renderer_vk {

enum class ShaderSemantic : std::uint8_t {
    None = 0,
    InputTexture,
    InputSize,
    OutputSize,
    OriginalSize,
    SourceSize,
    Time,
    DeltaTime,
    FrameCount,
    FrameDirection,
    ViewportSize,
    LastPassOutput,
    OriginalTexture,
    FilteredTexture,
    PassOutput
};

inline const char* shaderSemanticName(ShaderSemantic s) {
    switch (s) {
    case ShaderSemantic::None:            return "None";
    case ShaderSemantic::InputTexture:    return "InputTexture";
    case ShaderSemantic::InputSize:       return "InputSize";
    case ShaderSemantic::OutputSize:      return "OutputSize";
    case ShaderSemantic::OriginalSize:    return "OriginalSize";
    case ShaderSemantic::SourceSize:      return "SourceSize";
    case ShaderSemantic::Time:            return "Time";
    case ShaderSemantic::DeltaTime:       return "DeltaTime";
    case ShaderSemantic::FrameCount:      return "FrameCount";
    case ShaderSemantic::FrameDirection:  return "FrameDirection";
    case ShaderSemantic::ViewportSize:    return "ViewportSize";
    case ShaderSemantic::LastPassOutput:  return "LastPassOutput";
    case ShaderSemantic::OriginalTexture: return "OriginalTexture";
    case ShaderSemantic::FilteredTexture: return "FilteredTexture";
    case ShaderSemantic::PassOutput:      return "PassOutput";
    }
    return "Unknown";
}

struct SemanticAlias {
    ShaderSemantic semantic = ShaderSemantic::None;
    std::string originalName;
    std::string canonicalName;
};

struct SemanticMatch {
    ShaderSemantic semantic = ShaderSemantic::None;
    std::string originalName;
    bool exactMatch = false;
    bool aliasMatch = false;
    bool knownMatch = false;
};

class SemanticUniformResolver {
public:
    SemanticUniformResolver();

    SemanticMatch resolve(std::string_view uniformName) const;
    ShaderSemantic detectSemantic(std::string_view uniformName) const;
    std::string_view canonicalName(ShaderSemantic semantic) const;
    bool isKnownAlias(std::string_view name) const;

    void addAlias(ShaderSemantic semantic, std::string name);

    const std::vector<SemanticAlias>& aliases() const { return aliases_; }

    static SemanticUniformResolver makeDefault();

private:
    struct Entry {
        ShaderSemantic semantic;
        std::string name;
    };

    std::vector<Entry> entries_;
    std::vector<SemanticAlias> aliases_;
};

}  // namespace monix::renderer_vk
