#pragma once

#include "../core/Result.hpp"
#include "../shader_runtime/core/ShaderLanguage.hpp"
#include "ShaderReflection.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace monix::renderer_vk {

class ShaderCache;

enum class ShaderStage {
    Vertex,
    Fragment
};

struct ShaderCompileRequest {
    ShaderStage stage = ShaderStage::Fragment;
    ShaderLanguage language = ShaderLanguage::Slang;
    std::string source;
    std::filesystem::path sourcePath;
    std::filesystem::path outputDirectory;
    std::filesystem::path slangcPath;
    std::vector<std::filesystem::path> includeDirectories;
    std::vector<std::filesystem::path> dependencies;
    bool debugInfo = false;
};

struct ShaderCompileResult {
    std::vector<unsigned char> spirv;
    std::string reflectionJson;
    ShaderReflection reflection;
    std::string diagnostics;
    bool cacheHit = false;
};

class SlangCompiler {
public:
    Result<ShaderCompileResult> compile(const ShaderCompileRequest& request) const;
    static std::string stageName(ShaderStage stage);
    static std::string compilerVersion(const std::filesystem::path& slangcPath);

    void setCache(ShaderCache* cache) { cache_ = cache; }
    ShaderCache* cache() const { return cache_; }

private:
    ShaderCache* cache_ = nullptr;
};

}  // namespace monix::renderer_vk
