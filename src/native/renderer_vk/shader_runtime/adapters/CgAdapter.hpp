#pragma once

#include "IShaderLanguageAdapter.hpp"

namespace monix::renderer_vk {

class CgAdapter : public IShaderLanguageAdapter {
public:
    CgAdapter() = default;

    ShaderLanguage language() const override { return ShaderLanguage::CG; }
    const char* languageName() const override { return "CG"; }

    bool canCompile(ShaderLanguage lang) const override;

    Result<ShaderModule> compile(const ShaderRuntimeCompileRequest& request) const override;

    bool isAvailable() const override;
    std::string unavailabilityReason() const override;
};

}  // namespace monix::renderer_vk
