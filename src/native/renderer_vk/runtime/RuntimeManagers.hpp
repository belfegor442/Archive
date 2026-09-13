#pragma once

#include "../compiler/ShaderReflection.hpp"
#include "../graph/RenderGraph.hpp"
#include "../preset/ParameterExtractor.hpp"
#include "../public/RendererTypes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::renderer_vk {

struct StandardUniformValues {
    float mvp[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    Extent2D source;
    Extent2D original;
    Extent2D output;
    Extent2D finalViewport;
    uint32_t frameCount = 0;
    int frameDirection = 1;
};

class ParameterManager {
public:
    void registerParameters(const std::vector<ShaderParameter>& parameters);
    void set(std::string name, float value);
    float get(std::string_view name) const;
    const std::unordered_map<std::string, float>& values() const { return values_; }

private:
    std::unordered_map<std::string, ShaderParameter> definitions_;
    std::unordered_map<std::string, float> values_;
};

class UniformManager {
public:
    std::vector<unsigned char> buildUniformBuffer(const UniformBlockReflection& block,
                                                  const StandardUniformValues& standard,
                                                  const ParameterManager& parameters) const;

private:
    bool writeKnownUniform(std::vector<unsigned char>& buffer,
                           const UniformMemberReflection& member,
                           const StandardUniformValues& standard) const;
};

struct TextureResource {
    std::string name;
    Extent2D extent;
    bool external = false;
    bool mipmapped = false;
};

class TextureManager {
public:
    void registerTexture(TextureResource texture);
    const TextureResource* find(std::string_view name) const;
    const std::unordered_map<std::string, TextureResource>& textures() const { return textures_; }

private:
    std::unordered_map<std::string, TextureResource> textures_;
};

struct FeedbackState {
    std::string name;
    bool ping = false;
    bool initialized = false;
};

class FeedbackManager {
public:
    void registerFeedback(std::string name);
    void advance();
    const FeedbackState* find(std::string_view name) const;

private:
    std::unordered_map<std::string, FeedbackState> feedback_;
};

struct DescriptorAllocation {
    uint64_t poolId = 0;
    uint32_t setCount = 0;
};

class DescriptorPoolCache {
public:
    uint64_t acquirePool(uint32_t descriptorCount);

private:
    uint64_t nextPoolId_ = 1;
    std::vector<std::pair<uint64_t, uint32_t>> pools_;
};

class DescriptorAllocator {
public:
    explicit DescriptorAllocator(DescriptorPoolCache& pools);
    DescriptorAllocation allocate(uint32_t setCount, uint32_t descriptorCount);

private:
    DescriptorPoolCache& pools_;
};

class PipelineLayoutCache {
public:
    uint64_t getOrCreate(const ShaderReflection& reflection);
    size_t size() const { return layouts_.size(); }

private:
    uint64_t nextLayoutId_ = 1;
    std::unordered_map<std::string, uint64_t> layouts_;
};

class PipelineCache {
public:
    uint64_t getOrCreate(std::string key);
    size_t size() const { return pipelines_.size(); }

private:
    uint64_t nextPipelineId_ = 1;
    std::unordered_map<std::string, uint64_t> pipelines_;
};

class ResourceManager {
public:
    void importGraph(const CompiledGraph& graph);
    const TextureManager& textures() const { return textures_; }
    TextureManager& textures() { return textures_; }
    FeedbackManager& feedback() { return feedback_; }

private:
    TextureManager textures_;
    FeedbackManager feedback_;
};

}  // namespace monix::renderer_vk
