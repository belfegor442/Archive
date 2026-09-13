#include "CgAdapter.hpp"

namespace monix::renderer_vk {

bool CgAdapter::canCompile(ShaderLanguage lang) const {
    return lang == ShaderLanguage::CG;
}

bool CgAdapter::isAvailable() const {
    return false;
}

std::string CgAdapter::unavailabilityReason() const {
    return "NVIDIA Cg compiler is unavailable. "
           "The NVIDIA Cg Toolkit was discontinued in 2012. "
           "No compatible Cg compiler exists for modern systems. "
           "The shader was not compiled and was not activated. "
           "Do NOT convert CG to GLSL or Slang via text replacement. "
           "A future strategy may integrate a real Cg backend if one becomes available.";
}

Result<ShaderModule> CgAdapter::compile(const ShaderRuntimeCompileRequest& request) const {
    ShaderModule module;
    module.language = ShaderLanguage::CG;
    module.sourcePath = request.sourcePath.string();
    module.compiled = false;

    module.diagnostics = ShaderDiagnostics::makeUnsupported(
        "CG",
        unavailabilityReason()
    );

    return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
}

}  // namespace monix::renderer_vk
