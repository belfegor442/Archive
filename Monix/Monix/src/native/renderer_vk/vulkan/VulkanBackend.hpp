#pragma once

#include "../core/Result.hpp"
#include "../graph/RenderGraph.hpp"
#include "../api/RendererTypes.hpp"
#include "../compiler/CompiledPreset.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class VulkanRenderer;

namespace monix::renderer_vk {

struct VulkanBackendCapabilities {
    bool loaderAvailable = false;
    bool headersAvailableAtBuild = false;
    bool vmaAvailableAtBuild = false;
    bool dynamicRendering = false;
    bool timelineSemaphores = false;
    bool pipelineCache = false;
    bool debugUtils = false;
    bool descriptorIndexingPrepared = true;
};

class VulkanBackend {
public:
    VulkanBackend();
    ~VulkanBackend();

    VulkanBackend(const VulkanBackend&) = delete;
    VulkanBackend& operator=(const VulkanBackend&) = delete;

    const VulkanBackendCapabilities& capabilities() const { return capabilities_; }
    Status initialize();
    RuntimeBuildInfo buildInfo() const;

    void setRenderer(VulkanRenderer* renderer);
    VulkanRenderer* renderer() const { return renderer_; }

    Status loadCompiledPreset(const CompiledPreset& preset, uint32_t width, uint32_t height);
    void setCompiledPreset(const CompiledPreset* preset);
    void destroyPreset();

    Status execute(const ExecutionPlan& plan, uint32_t width, uint32_t height, const FrameImage* sourceFrame = nullptr);

    Status uploadSourceFrame(const FrameImage& frame);
    Status resize(uint32_t width, uint32_t height);

    uint32_t frameCount() const { return frameCount_; }

private:
    static std::vector<uint32_t> spirvToWords(const std::vector<unsigned char>& bytes);
    std::vector<float> buildPushDefaults(const CompiledPass& pass) const;
    uint32_t pickUboSize(const CompiledPass& pass) const;

    VulkanBackendCapabilities capabilities_;
    VulkanRenderer* renderer_ = nullptr;

    const CompiledPreset* compiled_ = nullptr;
    const CompiledGraph* graph_ = nullptr;
    uint32_t outputWidth_ = 0;
    uint32_t outputHeight_ = 0;
    bool presetLoaded_ = false;
    uint32_t frameCount_ = 0;

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace monix::renderer_vk
