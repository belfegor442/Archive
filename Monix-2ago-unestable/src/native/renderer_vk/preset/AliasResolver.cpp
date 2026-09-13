#include "AliasResolver.hpp"

#include <algorithm>
#include <cctype>

namespace monix::renderer_vk {
namespace {

bool endsWithInsensitive(std::string_view text, std::string_view suffix) {
    if (text.size() < suffix.size()) return false;
    const auto offset = text.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[offset + i])));
        const char b = static_cast<char>(std::tolower(static_cast<unsigned char>(suffix[i])));
        if (a != b) return false;
    }
    return true;
}

std::string removeFeedbackSuffix(std::string name) {
    constexpr std::string_view suffix = "Feedback";
    if (endsWithInsensitive(name, suffix)) {
        name.resize(name.size() - suffix.size());
    }
    return name;
}

}  // namespace

void AliasDatabase::addBuiltin(std::string name) {
    builtins_[std::move(name)] = true;
}

void AliasDatabase::addPassAlias(std::string alias, int passIndex) {
    if (!alias.empty()) passAliases_[std::move(alias)] = passIndex;
}

void AliasDatabase::addExternalTextureAlias(std::string alias, int textureIndex) {
    if (!alias.empty()) textureAliases_[std::move(alias)] = textureIndex;
}

AliasResolution AliasDatabase::resolve(std::string name, int requestingPass) const {
    if (builtins_.find(name) != builtins_.end()) {
        return AliasResolution{AliasKind::Builtin, std::move(name), -1, -1};
    }
    if (auto texture = textureAliases_.find(name); texture != textureAliases_.end()) {
        return AliasResolution{AliasKind::ExternalTexture, std::move(name), -1, texture->second};
    }
    if (auto pass = passAliases_.find(name); pass != passAliases_.end()) {
        if (pass->second < requestingPass) {
            return AliasResolution{AliasKind::PassOutput, std::move(name), pass->second, -1};
        }
        return AliasResolution{AliasKind::Unknown, std::move(name), pass->second, -1};
    }
    if (endsWithInsensitive(name, "Feedback")) {
        auto baseName = removeFeedbackSuffix(name);
        if (auto pass = passAliases_.find(baseName); pass != passAliases_.end()) {
            return AliasResolution{AliasKind::Feedback, std::move(baseName), pass->second, -1};
        }
        return AliasResolution{AliasKind::Feedback, std::move(name), -1, -1};
    }
    return AliasResolution{AliasKind::Unknown, std::move(name), -1, -1};
}

AliasDatabase AliasResolver::build(const PresetIr& preset) const {
    AliasDatabase db;
    db.addBuiltin("Source");
    db.addBuiltin("Original");
    db.addBuiltin("Texture");
    db.addBuiltin("ORIG_LINEARIZED");
    db.addBuiltin("PassOutput");
    db.addBuiltin("PreviousPass");

    for (const auto& pass : preset.passes) {
        db.addPassAlias(pass.alias, pass.index);
    }
    for (const auto& texture : preset.externalTextures) {
        db.addExternalTextureAlias(texture.alias, texture.textureIndex);
    }
    return db;
}

}  // namespace monix::renderer_vk
