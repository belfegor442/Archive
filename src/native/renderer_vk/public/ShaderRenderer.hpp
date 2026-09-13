#pragma once

#include "../compiler/CompiledPreset.hpp"
#include "../compiler/ShaderCache.hpp"
#include "../core/Result.hpp"
#include "../debug/RuntimeStatistics.hpp"
#include "../runtime/RuntimeManagers.hpp"
#include "../shader_runtime/core/ShaderModule.hpp"
#include "../shader_runtime/TransactionalShaderState.hpp"
#include "../shader_runtime/dependencies/ShaderDependencyGraph.hpp"
#include "../shader_runtime/reload/ShaderHotReload.hpp"
#include "../validation/GpuShaderValidator.hpp"
#include "../vulkan/VulkanBackend.hpp"
#include "RendererTypes.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace monix::renderer_vk {

class ShaderRenderer {
public:
    explicit ShaderRenderer(RendererConfig config);
    ~ShaderRenderer();

    ShaderRenderer(const ShaderRenderer&) = delete;
    ShaderRenderer& operator=(const ShaderRenderer&) = delete;

    Status initialize();
    void setRenderer(VulkanRenderer* renderer);
    Status loadPreset(const std::filesystem::path& slangpPath);
    Status loadIndividualShader(const ShaderModule& module, const std::filesystem::path& shaderPath);
    Status resize(uint32_t outputWidth, uint32_t outputHeight);
    Status uploadSourceFrame(const FrameImage& frame);
    Status setParameter(std::string_view name, float value);
    Status render(const FrameContext& frame, const FrameImage* sourceFrame = nullptr);

    Status enableHotReload();
    Status disableHotReload();
    Status pollHotReload();

    static void runCompilationTests();
    const CompiledPreset* compiledPreset() const;
    const RuntimeStatistics& statistics() const { return statistics_; }
    const VulkanBackendCapabilities& vulkanCapabilities() const { return backend_.capabilities(); }
    RuntimeBuildInfo buildInfo() const { return backend_.buildInfo(); }

    const TransactionalShaderState& transactional() const { return transactional_; }
    GpuShaderValidator& gpuValidator() { return gpuValidator_; }
    void setValidationProfile(const ShaderValidationProfile& profile) { gpuValidator_.setProfile(profile); }
    ShaderCache* shaderCache() const { return shaderCache_.get(); }

    ShaderDependencyGraph& dependencyGraph() { return depGraph_; }
    ShaderHotReload& hotReload() { return *hotReload_; }

private:
    Result<CompiledPreset> compilePreset(const std::filesystem::path& slangpPath);
    GpuTestDiagnostic runGpuValidationTest(const CompiledPreset& preset);
    void restorePreviousState();
    void rebuildStatistics();
    void registerDependencies(const CompiledPreset& preset);
    std::filesystem::path cacheDirectory() const;
    std::filesystem::path slangcPath() const;

    RendererConfig config_;
    VulkanBackend backend_;
    ParameterManager parameters_;
    ResourceManager resources_;
    DescriptorPoolCache descriptorPools_;
    PipelineLayoutCache pipelineLayouts_;
    PipelineCache pipelines_;
    std::optional<CompiledPreset> compiled_;
    TransactionalShaderState transactional_;
    GpuShaderValidator gpuValidator_;
    RuntimeStatistics statistics_;
    Extent2D outputExtent_;
    FrameImage lastSourceFrame_;
    ShaderDependencyGraph depGraph_;
    std::unique_ptr<ShaderHotReload> hotReload_;
    std::unique_ptr<ShaderCache> shaderCache_;
    std::filesystem::path loadedPresetPath_;
    std::mutex loadMutex_;
};

}  // namespace monix::renderer_vk
