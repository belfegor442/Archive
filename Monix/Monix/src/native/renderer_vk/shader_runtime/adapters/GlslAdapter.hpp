#pragma once

#include "IShaderLanguageAdapter.hpp"

namespace monix::renderer_vk {

class ShaderCache;

class GlslAdapter : public IShaderLanguageAdapter {
public:
    GlslAdapter() = default;

    ShaderLanguage language() const override { return ShaderLanguage::GLSL; }
    const char* languageName() const override { return "GLSL"; }

    bool canCompile(ShaderLanguage lang) const override;

    Result<ShaderModule> compile(const ShaderRuntimeCompileRequest& request) const override;

    bool isAvailable() const override;
    std::string unavailabilityReason() const override;

    void setCache(ShaderCache* cache) { cache_ = cache; }
    ShaderCache* cache() const { return cache_; }

private:
    ShaderCache* cache_ = nullptr;
};

}  // namespace monix::renderer_vk
