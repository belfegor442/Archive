#pragma once

#include "../core/ShaderLanguage.hpp"
#include "../core/ShaderModule.hpp"
#include "../core/ShaderDiagnostics.hpp"
#include "../../compiler/SlangCompiler.hpp"
#include "../../core/Result.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct ShaderRuntimeCompileRequest {
    ShaderLanguage language = ShaderLanguage::Unknown;
    std::string source;
    std::filesystem::path sourcePath;
    std::filesystem::path outputDirectory;
    std::filesystem::path compilerPath;
    std::vector<std::filesystem::path> includeDirectories;
    std::vector<std::filesystem::path> dependencies;
    bool debugInfo = false;
};

class IShaderLanguageAdapter {
public:
    virtual ~IShaderLanguageAdapter() = default;

    virtual ShaderLanguage language() const = 0;
    virtual const char* languageName() const = 0;

    virtual bool canCompile(ShaderLanguage lang) const = 0;

    virtual Result<ShaderModule> compile(const ShaderRuntimeCompileRequest& request) const = 0;

    virtual bool isAvailable() const = 0;
    virtual std::string unavailabilityReason() const = 0;
};

}  // namespace monix::renderer_vk
