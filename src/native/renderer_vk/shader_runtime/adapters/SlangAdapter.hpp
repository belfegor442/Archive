#pragma once

#include "IShaderLanguageAdapter.hpp"

namespace monix::renderer_vk {

class ShaderCache;

class SlangAdapter : public IShaderLanguageAdapter {
public:
    SlangAdapter() = default;

    ShaderLanguage language() const override { return ShaderLanguage::Slang; }
    const char* languageName() const override { return "Slang"; }

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
