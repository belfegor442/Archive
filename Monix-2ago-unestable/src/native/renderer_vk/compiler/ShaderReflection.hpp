#pragma once

#include "../core/Result.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace monix::renderer_vk {

enum class ReflectedResourceKind {
    UniformBuffer,
    CombinedImageSampler,
    StorageBuffer,
    StorageImage
};

struct DescriptorBindingReflection {
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
    ReflectedResourceKind kind = ReflectedResourceKind::UniformBuffer;
};

struct UniformMemberReflection {
    std::string name;
    std::string type;
    uint32_t offset = 0;
    uint32_t size = 0;
};

struct UniformBlockReflection {
    std::string name;
    std::string instanceName;
    uint32_t set = 0;
    uint32_t binding = 0;
    uint32_t size = 0;
    std::vector<UniformMemberReflection> members;
};

struct PushConstantReflection {
    std::string name;
    uint32_t size = 0;
    std::vector<UniformMemberReflection> members;
};

struct SamplerReflection {
    std::string name;
    uint32_t set = 0;
    uint32_t binding = 0;
};

struct ShaderReflection {
    std::vector<DescriptorBindingReflection> descriptors;
    std::vector<UniformBlockReflection> uniformBlocks;
    std::vector<PushConstantReflection> pushConstants;
    std::vector<SamplerReflection> samplers;
    bool fallbackSourceReflection = false;

    const UniformMemberReflection* findUniform(std::string_view name) const;
    const SamplerReflection* findSampler(std::string_view name) const;
};

class ShaderReflectionParser {
public:
    Result<ShaderReflection> fromSourceLayout(const std::string& source) const;
    Result<ShaderReflection> fromSlangJson(const std::string& json, const std::string& sourceFallback) const;
};

}  // namespace monix::renderer_vk
