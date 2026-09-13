#pragma once

#include "SlangPreset.hpp"

#include <optional>
#include <string>
#include <unordered_map>

namespace monix::renderer_vk {

enum class AliasKind {
    Builtin,
    PassOutput,
    ExternalTexture,
    Feedback,
    Unknown
};

struct AliasResolution {
    AliasKind kind = AliasKind::Unknown;
    std::string name;
    int passIndex = -1;
    int textureIndex = -1;
};

class AliasDatabase {
public:
    void addBuiltin(std::string name);
    void addPassAlias(std::string alias, int passIndex);
    void addExternalTextureAlias(std::string alias, int textureIndex);

    AliasResolution resolve(std::string name, int requestingPass) const;
    const std::unordered_map<std::string, int>& passAliases() const { return passAliases_; }
    const std::unordered_map<std::string, int>& textureAliases() const { return textureAliases_; }

private:
    std::unordered_map<std::string, int> passAliases_;
    std::unordered_map<std::string, int> textureAliases_;
    std::unordered_map<std::string, bool> builtins_;
};

class AliasResolver {
public:
    AliasDatabase build(const PresetIr& preset) const;
};

}  // namespace monix::renderer_vk
