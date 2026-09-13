#pragma once

#include "ShaderLanguage.hpp"
#include "ShaderStage.hpp"
#include "ShaderDiagnostics.hpp"
#include "../../compiler/ShaderReflection.hpp"
#include "../../core/Result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace monix::renderer_vk {

struct ShaderModuleStage {
    ShaderStage stage = ShaderStage::Fragment;
    std::string source;
    std::vector<unsigned char> spirv;
    ShaderReflection reflection;
};

struct ShaderModule {
    ShaderLanguage language = ShaderLanguage::Unknown;
    std::string sourcePath;
    std::vector<ShaderModuleStage> stages;
    ShaderReflection reflection;
    ShaderDiagnostics diagnostics;
    bool compiled = false;
    bool cacheHit = false;  // 15.2 — populated by adapter from SlangCompiler

    bool hasVertex() const {
        for (const auto& s : stages) {
            if (s.stage == ShaderStage::Vertex) return true;
        }
        return false;
    }

    bool hasFragment() const {
        for (const auto& s : stages) {
            if (s.stage == ShaderStage::Fragment) return true;
        }
        return false;
    }

    const ShaderModuleStage* getStage(ShaderStage stage) const {
        for (const auto& s : stages) {
            if (s.stage == stage) return &s;
        }
        return nullptr;
    }

    std::size_t totalSpirvBytes() const {
        std::size_t total = 0;
        for (const auto& s : stages) total += s.spirv.size();
        return total;
    }
};

}  // namespace monix::renderer_vk
