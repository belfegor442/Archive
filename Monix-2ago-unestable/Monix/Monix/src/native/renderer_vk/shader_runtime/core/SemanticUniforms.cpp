#include "SemanticUniforms.hpp"

#include <algorithm>
#include <cctype>

namespace monix::renderer_vk {
namespace {

std::string toLower(std::string_view sv) {
    std::string result(sv);
    for (auto& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}

}  // namespace

SemanticUniformResolver::SemanticUniformResolver() = default;

SemanticMatch SemanticUniformResolver::resolve(std::string_view uniformName) const {
    SemanticMatch match;
    std::string lower = toLower(uniformName);

    for (const auto& entry : entries_) {
        if (entry.name == lower) {
            match.semantic = entry.semantic;
            match.originalName = std::string(uniformName);
            match.exactMatch = true;
            return match;
        }
    }

    for (const auto& alias : aliases_) {
        std::string aliasLower = toLower(alias.canonicalName);
        if (aliasLower == lower) {
            match.semantic = alias.semantic;
            match.originalName = std::string(uniformName);
            match.aliasMatch = true;
            return match;
        }
    }

    for (const auto& entry : entries_) {
        if (lower.find(entry.name) != std::string::npos) {
            match.semantic = entry.semantic;
            match.originalName = std::string(uniformName);
            match.knownMatch = true;
            return match;
        }
    }

    return match;
}

ShaderSemantic SemanticUniformResolver::detectSemantic(std::string_view uniformName) const {
    return resolve(uniformName).semantic;
}

std::string_view SemanticUniformResolver::canonicalName(ShaderSemantic semantic) const {
    for (const auto& entry : entries_) {
        if (entry.semantic == semantic) return entry.name;
    }
    return "";
}

bool SemanticUniformResolver::isKnownAlias(std::string_view name) const {
    std::string lower = toLower(name);
    for (const auto& entry : entries_) {
        if (entry.name == lower) return true;
    }
    for (const auto& alias : aliases_) {
        std::string aliasLower = toLower(alias.canonicalName);
        if (aliasLower == lower) return true;
    }
    return false;
}

void SemanticUniformResolver::addAlias(ShaderSemantic semantic, std::string name) {
    SemanticAlias alias;
    alias.semantic = semantic;
    alias.originalName = std::move(name);
    alias.canonicalName = toLower(alias.originalName);
    aliases_.push_back(std::move(alias));
}

SemanticUniformResolver SemanticUniformResolver::makeDefault() {
    SemanticUniformResolver resolver;

    resolver.entries_.push_back({ShaderSemantic::InputTexture, "inputtexture"});
    resolver.entries_.push_back({ShaderSemantic::InputSize, "inputsize"});
    resolver.entries_.push_back({ShaderSemantic::OutputSize, "outputsize"});
    resolver.entries_.push_back({ShaderSemantic::OriginalSize, "originalsize"});
    resolver.entries_.push_back({ShaderSemantic::SourceSize, "sourcesize"});
    resolver.entries_.push_back({ShaderSemantic::Time, "time"});
    resolver.entries_.push_back({ShaderSemantic::DeltaTime, "deltatime"});
    resolver.entries_.push_back({ShaderSemantic::FrameCount, "framecount"});
    resolver.entries_.push_back({ShaderSemantic::FrameDirection, "framedirection"});
    resolver.entries_.push_back({ShaderSemantic::ViewportSize, "viewportsize"});
    resolver.entries_.push_back({ShaderSemantic::LastPassOutput, "lastpassoutput"});
    resolver.entries_.push_back({ShaderSemantic::OriginalTexture, "originaltexture"});
    resolver.entries_.push_back({ShaderSemantic::FilteredTexture, "filteredtexture"});
    resolver.entries_.push_back({ShaderSemantic::PassOutput, "passoutput"});

    resolver.addAlias(ShaderSemantic::InputTexture, "Source");
    resolver.addAlias(ShaderSemantic::InputTexture, "source");
    resolver.addAlias(ShaderSemantic::InputTexture, "InputTexture");
    resolver.addAlias(ShaderSemantic::InputTexture, "SourceTexture");
    resolver.addAlias(ShaderSemantic::InputTexture, "sourceTexture");
    resolver.addAlias(ShaderSemantic::InputTexture, "inputTexture");

    resolver.addAlias(ShaderSemantic::InputSize, "SourceSize");
    resolver.addAlias(ShaderSemantic::InputSize, "sourceSize");
    resolver.addAlias(ShaderSemantic::InputSize, "InputSize");
    resolver.addAlias(ShaderSemantic::InputSize, "inputSize");

    resolver.addAlias(ShaderSemantic::OutputSize, "OutputSize");
    resolver.addAlias(ShaderSemantic::OutputSize, "outputSize");

    resolver.addAlias(ShaderSemantic::OriginalSize, "OriginalSize");
    resolver.addAlias(ShaderSemantic::OriginalSize, "originalSize");

    resolver.addAlias(ShaderSemantic::SourceSize, "source_size");
    resolver.addAlias(ShaderSemantic::SourceSize, "Source_size");

    resolver.addAlias(ShaderSemantic::Time, "Timer");
    resolver.addAlias(ShaderSemantic::Time, "timer");
    resolver.addAlias(ShaderSemantic::Time, "ElapsedTime");

    resolver.addAlias(ShaderSemantic::DeltaTime, "delta_time");
    resolver.addAlias(ShaderSemantic::DeltaTime, "DeltaTime");
    resolver.addAlias(ShaderSemantic::DeltaTime, "FrameDelta");

    resolver.addAlias(ShaderSemantic::FrameCount, "frame_count");
    resolver.addAlias(ShaderSemantic::FrameCount, "FrameCount");
    resolver.addAlias(ShaderSemantic::FrameCount, "FrameCounter");

    resolver.addAlias(ShaderSemantic::FrameDirection, "frame_direction");
    resolver.addAlias(ShaderSemantic::FrameDirection, "FrameDirection");
    resolver.addAlias(ShaderSemantic::FrameDirection, "direction");

    resolver.addAlias(ShaderSemantic::ViewportSize, "viewport_size");
    resolver.addAlias(ShaderSemantic::ViewportSize, "ViewportSize");
    resolver.addAlias(ShaderSemantic::ViewportSize, "Viewport");

    resolver.addAlias(ShaderSemantic::OriginalTexture, "Original");
    resolver.addAlias(ShaderSemantic::OriginalTexture, "original");
    resolver.addAlias(ShaderSemantic::OriginalTexture, "OriginalTexture");
    resolver.addAlias(ShaderSemantic::OriginalTexture, "originalTexture");

    resolver.addAlias(ShaderSemantic::FilteredTexture, "Filtered");
    resolver.addAlias(ShaderSemantic::FilteredTexture, "filtered");
    resolver.addAlias(ShaderSemantic::FilteredTexture, "FilterTexture");

    resolver.addAlias(ShaderSemantic::PassOutput, "PassOutput");
    resolver.addAlias(ShaderSemantic::PassOutput, "pass_output");
    resolver.addAlias(ShaderSemantic::PassOutput, "PreviousPass");
    resolver.addAlias(ShaderSemantic::PassOutput, "previousPass");

    return resolver;
}

}  // namespace monix::renderer_vk
