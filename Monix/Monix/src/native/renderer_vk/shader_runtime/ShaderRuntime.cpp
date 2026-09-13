#include "ShaderRuntime.hpp"

#include "adapters/SlangAdapter.hpp"
#include "adapters/GlslAdapter.hpp"
#include "adapters/CgAdapter.hpp"
#include "../compiler/ShaderCache.hpp"

#include <fstream>
#include <sstream>

namespace monix::renderer_vk {

ShaderRuntime::ShaderRuntime(ShaderRuntimeConfig config)
    : config_(std::move(config))
{
}

ShaderRuntime::~ShaderRuntime() = default;

void ShaderRuntime::initialize() {
    adapters_.clear();

    auto slang = std::make_unique<SlangAdapter>();
    slang->setCache(cache_);
    adapters_.push_back(std::move(slang));

    auto glsl = std::make_unique<GlslAdapter>();
    glsl->setCache(cache_);
    adapters_.push_back(std::move(glsl));

    adapters_.push_back(std::make_unique<CgAdapter>());
}

ShaderLanguage ShaderRuntime::detectLanguage(const std::filesystem::path& path) {
    auto ext = path.extension().string();
    return detectLanguageFromExtension(ext);
}

IShaderLanguageAdapter* ShaderRuntime::adapter(ShaderLanguage language) const {
    for (const auto& a : adapters_) {
        if (a->language() == language) return a.get();
    }
    return nullptr;
}

IShaderLanguageAdapter* ShaderRuntime::adapterForFile(const std::filesystem::path& path) const {
    auto lang = detectLanguage(path);
    return adapter(lang);
}

bool ShaderRuntime::isLanguageSupported(ShaderLanguage lang) const {
    return adapter(lang) != nullptr;
}

bool ShaderRuntime::isLanguageAvailable(ShaderLanguage lang) const {
    auto* a = adapter(lang);
    return a && a->isAvailable();
}

std::string ShaderRuntime::languageAvailability(ShaderLanguage lang) const {
    auto* a = adapter(lang);
    if (!a) return std::string(shaderLanguageName(lang)) + " is not supported by any adapter.";
    if (a->isAvailable()) return std::string(shaderLanguageName(lang)) + " is available.";
    return std::string(shaderLanguageName(lang)) + " is not available: " + a->unavailabilityReason();
}

Result<ShaderModule> ShaderRuntime::compileShader(
    ShaderLanguage language,
    const std::filesystem::path& sourcePath,
    const std::string& source) const
{
    auto* a = adapter(language);
    if (!a) {
        {
            std::lock_guard lock(cacheStatusMutex_);
            lastCacheStatus_ = "NONE";
        }
        return Result<ShaderModule>(Status::failure(
            std::string(shaderLanguageName(language)) + " is not supported by any adapter."));
    }

    if (!a->isAvailable()) {
        {
            std::lock_guard lock(cacheStatusMutex_);
            lastCacheStatus_ = "NONE";
        }
        ShaderModule module;
        module.language = language;
        module.sourcePath = sourcePath.string();
        module.compiled = false;
        module.diagnostics = ShaderDiagnostics::makeUnsupported(
            a->languageName(), a->unavailabilityReason());
        return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
    }

    std::string actualSource = source;
    if (actualSource.empty()) {
        std::ifstream file(sourcePath, std::ios::binary);
        if (!file.is_open()) {
            {
                std::lock_guard lock(cacheStatusMutex_);
                lastCacheStatus_ = "NONE";
            }
            ShaderModule module;
            module.language = language;
            module.sourcePath = sourcePath.string();
            module.compiled = false;
            module.diagnostics = ShaderDiagnostics::makeError(
                ShaderDiagnosticKind::LoadError,
                sourcePath.string(),
                0,
                "Failed to open file: " + sourcePath.string()
            );
            return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
        }
        std::ostringstream oss;
        oss << file.rdbuf();
        actualSource = oss.str();
    }

    ShaderRuntimeCompileRequest request;
    request.language = language;
    request.source = std::move(actualSource);
    request.sourcePath = sourcePath;
    request.outputDirectory = config_.shaderCacheDirectory;
    request.compilerPath = config_.slangcPath;
    request.includeDirectories = config_.includeDirectories;
    request.debugInfo = config_.debugInfo;

    auto result = a->compile(request);

    // 15.2 — propagate cache status from module
    {
        std::lock_guard lock(cacheStatusMutex_);
        if (result) {
            const auto& mod = result.value();
            if (mod.compiled) {
                lastCacheStatus_ = mod.cacheHit ? "HIT" : "MISS";
            } else {
                lastCacheStatus_ = "MISS";
            }
        } else {
            lastCacheStatus_ = "MISS";
        }
    }

    return result;
}

Result<ShaderModule> ShaderRuntime::compileShaderFromSource(
    ShaderLanguage language,
    const std::string& source,
    const std::filesystem::path& sourcePath,
    const std::vector<std::filesystem::path>& includeDirectories,
    const std::vector<std::filesystem::path>& dependencies) const
{
    auto* a = adapter(language);
    if (!a) {
        return Result<ShaderModule>(Status::failure(
            std::string(shaderLanguageName(language)) + " is not supported by any adapter."));
    }

    if (!a->isAvailable()) {
        ShaderModule module;
        module.language = language;
        module.sourcePath = sourcePath.string();
        module.compiled = false;
        module.diagnostics = ShaderDiagnostics::makeUnsupported(
            a->languageName(), a->unavailabilityReason());
        return Result<ShaderModule>(Status::failure(module.diagnostics.formatAll()));
    }

    ShaderRuntimeCompileRequest request;
    request.language = language;
    request.source = source;
    request.sourcePath = sourcePath;
    request.outputDirectory = config_.shaderCacheDirectory;
    request.compilerPath = config_.slangcPath;
    request.includeDirectories = includeDirectories;
    request.dependencies = dependencies;
    request.debugInfo = config_.debugInfo;

    return a->compile(request);
}

}  // namespace monix::renderer_vk
