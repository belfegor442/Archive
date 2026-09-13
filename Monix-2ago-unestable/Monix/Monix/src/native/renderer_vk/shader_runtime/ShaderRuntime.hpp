#pragma once

#include "core/ShaderLanguage.hpp"
#include "core/ShaderStage.hpp"
#include "core/ShaderModule.hpp"
#include "core/ShaderDiagnostics.hpp"
#include "adapters/IShaderLanguageAdapter.hpp"
#include "../compiler/CompiledPreset.hpp"
#include "../compiler/ShaderCache.hpp"
#include "../core/Result.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct ShaderRuntimeConfig {
    std::filesystem::path rootDirectory;
    std::filesystem::path shaderCacheDirectory;
    std::filesystem::path slangcPath;
    std::vector<std::filesystem::path> includeDirectories;
    bool compileShadersToSpirv = true;
    bool debugInfo = false;
};

class ShaderRuntime {
public:
    explicit ShaderRuntime(ShaderRuntimeConfig config);
    ~ShaderRuntime();

    ShaderRuntime(const ShaderRuntime&) = delete;
    ShaderRuntime& operator=(const ShaderRuntime&) = delete;

    void initialize();

    Result<ShaderModule> compileShader(
        ShaderLanguage language,
        const std::filesystem::path& sourcePath,
        const std::string& source = ""
    ) const;

    Result<ShaderModule> compileShaderFromSource(
        ShaderLanguage language,
        const std::string& source,
        const std::filesystem::path& sourcePath,
        const std::vector<std::filesystem::path>& includeDirectories = {},
        const std::vector<std::filesystem::path>& dependencies = {}
    ) const;

    IShaderLanguageAdapter* adapter(ShaderLanguage language) const;
    IShaderLanguageAdapter* adapterForFile(const std::filesystem::path& path) const;

    bool isLanguageSupported(ShaderLanguage lang) const;
    bool isLanguageAvailable(ShaderLanguage lang) const;
    std::string languageAvailability(ShaderLanguage lang) const;

    static ShaderLanguage detectLanguage(const std::filesystem::path& path);

    const ShaderRuntimeConfig& config() const { return config_; }

    void setCache(ShaderCache* cache) { cache_ = cache; }
    const ShaderCache* cache() const { return cache_; }

    // 15 — cache status from last compile
    const std::string lastCacheStatus() const { std::lock_guard lock(cacheStatusMutex_); return lastCacheStatus_; }
    void setLastCacheStatus(const std::string& s) const { std::lock_guard lock(cacheStatusMutex_); lastCacheStatus_ = s; }

private:
    ShaderRuntimeConfig config_;
    std::vector<std::unique_ptr<IShaderLanguageAdapter>> adapters_;
    ShaderCache* cache_ = nullptr;
    mutable std::mutex cacheStatusMutex_;
    mutable std::string lastCacheStatus_ = "NONE";
};

}  // namespace monix::renderer_vk
