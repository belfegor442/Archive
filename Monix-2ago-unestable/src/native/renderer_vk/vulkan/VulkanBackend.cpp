#include "VulkanBackend.hpp"

#include <cstring>
#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "vulkan_renderer.h"

namespace monix::renderer_vk {

struct VulkanBackend::Impl {
    struct ResolvedSampler {
        uint32_t binding = 0;
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkImage image = VK_NULL_HANDLE;
    };

    struct ResolvedPassBindings {
        std::vector<ResolvedSampler> samplers;
    };

    std::vector<ResolvedPassBindings> resolvedBindings;

    void resolvePassBindings(size_t passIdx, const CompiledPreset* compiled,
                             const CompiledGraph* graph, VulkanRenderer* renderer,
                             ResolvedPassBindings& out) const;
};

void VulkanBackend::Impl::resolvePassBindings(
    size_t passIdx, const CompiledPreset* compiled,
    const CompiledGraph* graph, VulkanRenderer* renderer,
    ResolvedPassBindings& out) const {

    out.samplers.clear();
    if (!graph || passIdx >= graph->passes.size()) return;
    if (passIdx >= compiled->passes.size()) return;

    const auto& graphPass = graph->passes[passIdx];
    const auto& fragRefl = compiled->passes[passIdx].fragment.reflection;

    VkSampler linearSampler = VK_NULL_HANDLE;
    if (renderer && !renderer->currentPreset().samplers.empty()) {
        linearSampler = renderer->currentPreset().samplers[0].sampler;
    }

    for (const auto& sampler : fragRefl.samplers) {
        ResolvedSampler rs{};
        rs.binding = sampler.binding;
        rs.sampler = linearSampler;

        const PassDependency* dep = nullptr;
        for (const auto& input : graphPass.inputs) {
            if (input.samplerName == sampler.name) {
                dep = &input;
                break;
            }
        }

        if (!dep) {
            out.samplers.push_back(rs);
            continue;
        }

        const ImageNode* imageNode = nullptr;
        for (const auto& img : graph->images) {
            if (img.id == dep->resourceId) {
                imageNode = &img;
                break;
            }
        }

        if (!imageNode) {
            out.samplers.push_back(rs);
            continue;
        }

        if (imageNode->external) {
            rs.imageView = renderer->sourceImageView();
            rs.image = renderer->sourceImageResource().image;
        } else if (imageNode->feedback) {
            rs.imageView = renderer->sourceImageView();
            rs.image = renderer->sourceImageResource().image;
        } else if (imageNode->producerPass >= 0 &&
                   imageNode->producerPass < static_cast<int>(renderer->currentPreset().passes.size())) {
            const auto& producer = renderer->currentPreset().passes[imageNode->producerPass];
            rs.imageView = producer.renderTarget.view;
            rs.image = producer.renderTarget.image;
        }

        out.samplers.push_back(rs);
    }
}

VulkanBackend::VulkanBackend() : impl_(std::make_unique<Impl>()) {}
VulkanBackend::~VulkanBackend() = default;

Status VulkanBackend::initialize() {
    capabilities_.loaderAvailable = true;
#ifdef MONIX_RENDERER_HAS_VULKAN_HEADERS
    capabilities_.headersAvailableAtBuild = true;
#else
    capabilities_.headersAvailableAtBuild = false;
#endif
#ifdef MONIX_RENDERER_HAS_VMA
    capabilities_.vmaAvailableAtBuild = true;
#else
    capabilities_.vmaAvailableAtBuild = false;
#endif
    capabilities_.dynamicRendering = capabilities_.headersAvailableAtBuild;
    capabilities_.timelineSemaphores = capabilities_.dynamicRendering;
    capabilities_.pipelineCache = true;
    capabilities_.debugUtils = true;
    capabilities_.descriptorIndexingPrepared = true;
    return Status::success();
}

RuntimeBuildInfo VulkanBackend::buildInfo() const {
    RuntimeBuildInfo info;
    info.vulkanHeadersAvailable = capabilities_.headersAvailableAtBuild;
    info.vmaHeadersAvailable = capabilities_.vmaAvailableAtBuild;
    info.compiler = "MSVC C++20";
    return info;
}

void VulkanBackend::setRenderer(VulkanRenderer* renderer) {
    renderer_ = renderer;
}

std::vector<uint32_t> VulkanBackend::spirvToWords(const std::vector<unsigned char>& bytes) {
    if (bytes.empty()) return {};
    const size_t wordCount = bytes.size() / 4;
    std::vector<uint32_t> words(wordCount);
    std::memcpy(words.data(), bytes.data(), wordCount * 4);
    return words;
}

uint32_t VulkanBackend::pickUboSize(const CompiledPass& pass) const {
    const auto& refl = pass.fragment.reflection;
    if (!refl.uniformBlocks.empty()) {
        return refl.uniformBlocks[0].size;
    }
    if (!refl.descriptors.empty()) {
        for (const auto& d : refl.descriptors) {
            if (d.kind == ReflectedResourceKind::UniformBuffer) {
                return 128;
            }
        }
    }
    return sizeof(UniformData);
}

std::vector<float> VulkanBackend::buildPushDefaults(const CompiledPass& pass) const {
    const auto& refl = pass.fragment.reflection;
    if (refl.pushConstants.empty()) return {};

    const auto& pc = refl.pushConstants[0];
    if (pc.size == 0) return {};

    std::vector<float> defaults(pc.size / sizeof(float), 0.0f);

    for (const auto& member : pc.members) {
        if (member.size != sizeof(float)) continue;

        const ShaderParameter* param = nullptr;
        for (const auto& p : pass.parameters) {
            if (p.name == member.name) {
                param = &p;
                break;
            }
        }

        float value = 0.0f;
        if (param) {
            value = param->defaultValue;
        }

        uint32_t floatIndex = member.offset / sizeof(float);
        if (floatIndex < defaults.size()) {
            defaults[floatIndex] = value;
        }
    }

    return defaults;
}

// ============================================================================
// loadCompiledPreset — convert CompiledPreset → VulkanRenderer GPU resources
// using reflection-driven descriptor layouts
// ============================================================================
Status VulkanBackend::loadCompiledPreset(const CompiledPreset& preset, uint32_t width, uint32_t height) {
    if (!renderer_) return Status::failure("No renderer set");
    if (preset.passes.empty()) return Status::failure("Empty preset");

    destroyPreset();
    compiled_ = &preset;
    graph_ = &preset.graph;
    outputWidth_ = width;
    outputHeight_ = height;

    const size_t passCount = preset.passes.size();

    std::vector<std::vector<uint32_t>> vertSpvs(passCount);
    std::vector<std::vector<uint32_t>> fragSpvs(passCount);
    std::vector<uint32_t> pushSizes(passCount, 0);
    std::vector<VkFormat> rtFormats(passCount, renderer_->swapchainFormat());
    std::vector<std::vector<float>> pushDefaultsList(passCount);
    std::vector<uint32_t> uboSizes(passCount, sizeof(UniformData));
    std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassSamplerBindings(passCount);

    for (size_t i = 0; i < passCount; ++i) {
        const auto& pass = preset.passes[i];

        if (!pass.vertex.spirv.empty()) {
            vertSpvs[i] = spirvToWords(pass.vertex.spirv);
        }
        if (!pass.fragment.spirv.empty()) {
            fragSpvs[i] = spirvToWords(pass.fragment.spirv);
        }

        if (!pass.fragment.reflection.pushConstants.empty()) {
            pushSizes[i] = pass.fragment.reflection.pushConstants[0].size;
        }

        if (pass.preset.floatFramebuffer) {
            rtFormats[i] = VK_FORMAT_R32G32B32A32_SFLOAT;
        } else if (pass.preset.srgbFramebuffer) {
            rtFormats[i] = VK_FORMAT_R8G8B8A8_SRGB;
        }

        pushDefaultsList[i] = buildPushDefaults(pass);
        uboSizes[i] = pickUboSize(pass);

        for (const auto& sampler : pass.fragment.reflection.samplers) {
            perPassSamplerBindings[i].push_back(
                VulkanRenderer::SamplerBindingInfo{sampler.binding, sampler.name});
        }
    }

    bool ok = renderer_->loadPresetReflection(
        width, height,
        vertSpvs, fragSpvs,
        pushSizes, rtFormats,
        pushDefaultsList, uboSizes,
        perPassSamplerBindings);

    if (!ok) {
        return Status::failure("VulkanRenderer::loadPresetReflection failed");
    }

    impl_->resolvedBindings.resize(passCount);
    for (size_t i = 0; i < passCount; ++i) {
        impl_->resolvePassBindings(i, compiled_, graph_, renderer_, impl_->resolvedBindings[i]);
    }

    presetLoaded_ = true;
    return Status::success();
}

void VulkanBackend::setCompiledPreset(const CompiledPreset* preset) {
    compiled_ = preset;
    graph_ = preset ? &preset->graph : nullptr;
}

void VulkanBackend::destroyPreset() {
    if (renderer_ && presetLoaded_) {
        renderer_->destroyPreset();
    }
    compiled_ = nullptr;
    graph_ = nullptr;
    impl_->resolvedBindings.clear();
    presetLoaded_ = false;
}

Status VulkanBackend::uploadSourceFrame(const FrameImage& frame) {
    if (!renderer_) return Status::failure("No renderer");
    if (!frame.pixels || frame.bytes == 0) return Status::failure("Invalid frame data");

    bool ok = renderer_->uploadTexture(frame.pixels, frame.extent.width, frame.extent.height);
    return ok ? Status::success() : Status::failure("uploadTexture failed");
}

Status VulkanBackend::resize(uint32_t width, uint32_t height) {
    if (!renderer_) return Status::failure("No renderer");
    outputWidth_ = width;
    outputHeight_ = height;
    renderer_->resize(width, height);

    if (presetLoaded_ && compiled_) {
        return loadCompiledPreset(*compiled_, width, height);
    }
    return Status::success();
}

// ============================================================================
// execute — render all passes using resolved graph-driven bindings
// ============================================================================
Status VulkanBackend::execute(const ExecutionPlan& plan, uint32_t width, uint32_t height, const FrameImage* sourceFrame) {
    if (!renderer_) return Status::failure("No renderer");
    if (!presetLoaded_) return Status::failure("No preset loaded");
    if (!renderer_->isInitialized()) return Status::failure("Renderer not initialized");

    outputWidth_ = width;
    outputHeight_ = height;

    if (sourceFrame && sourceFrame->pixels && sourceFrame->bytes > 0) {
        if (!renderer_->uploadTexture(sourceFrame->pixels, sourceFrame->extent.width, sourceFrame->extent.height)) {
            return Status::failure("uploadTexture for sourceFrame failed");
        }
    }

    if (!renderer_->beginFrame(width, height)) {
        return Status::failure("beginFrame failed");
    }

    const auto& preset = renderer_->currentPreset();
    if (!preset.valid()) {
        renderer_->endFrame();
        return Status::failure("No valid preset in renderer");
    }

    const auto& passOrder = plan.passOrder;
    const size_t totalPasses = passOrder.size();

    for (size_t idx = 0; idx < totalPasses; ++idx) {
        const int passIdx = passOrder[idx];
        if (passIdx < 0 || passIdx >= static_cast<int>(preset.passes.size())) continue;

        auto& pass = const_cast<VulkanRenderer::PresetPassResources&>(preset.passes[passIdx]);
        const bool isLastPass = (idx == totalPasses - 1);

        VkImageView rtView;
        uint32_t rtW, rtH;
        if (isLastPass) {
            rtView = renderer_->currentSwapchainView();
            rtW = width;
            rtH = height;
        } else {
            rtView = pass.renderTarget.view;
            rtW = pass.width;
            rtH = pass.height;
        }

        if (isLastPass) {
            renderer_->cmdTransitionImage(renderer_->currentSwapchainImage(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        } else if (pass.renderTarget.image) {
            renderer_->cmdTransitionImage(pass.renderTarget.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        if (pass.descriptorSet && passIdx < static_cast<int>(impl_->resolvedBindings.size())) {
            const auto& resolved = impl_->resolvedBindings[passIdx];
            for (const auto& rs : resolved.samplers) {
                if (rs.binding > 0 && rs.imageView != VK_NULL_HANDLE) {
                    renderer_->updateDescriptorSetTexture(pass.descriptorSet,
                        rs.binding, rs.imageView, rs.sampler);
                }
            }
        }

        VkClearValue clear{};
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        renderer_->cmdBeginRendering(rtView, rtW, rtH, clear);

        renderer_->cmdBindPipeline(pass.pipeline, pass.pipelineLayout);

        renderer_->cmdSetViewport(rtW, rtH);
        renderer_->cmdSetScissor(rtW, rtH);

        struct PresetUBO {
            float mvp[16];
            float outputSize[4];
            float originalSize[4];
            float sourceSize[4];
        } ubo{};
        ubo.mvp[0] = 1.0f;
        ubo.mvp[5] = 1.0f;
        ubo.mvp[10] = 1.0f;
        ubo.mvp[15] = 1.0f;
        ubo.outputSize[0] = static_cast<float>(rtW);
        ubo.outputSize[1] = static_cast<float>(rtH);
        ubo.outputSize[2] = 1.0f / static_cast<float>(rtW);
        ubo.outputSize[3] = 1.0f / static_cast<float>(rtH);
        ubo.originalSize[0] = static_cast<float>(width);
        ubo.originalSize[1] = static_cast<float>(height);
        ubo.originalSize[2] = 1.0f / static_cast<float>(width);
        ubo.originalSize[3] = 1.0f / static_cast<float>(height);

        float srcW = static_cast<float>(rtW);
        float srcH = static_cast<float>(rtH);
        ubo.sourceSize[0] = srcW;
        ubo.sourceSize[1] = srcH;
        ubo.sourceSize[2] = 1.0f / srcW;
        ubo.sourceSize[3] = 1.0f / srcH;

        renderer_->uploadBuffer(pass.uniformBuffer, &ubo, sizeof(ubo));

        if (pass.pushConstantSize > 0) {
            std::vector<uint8_t> pushData(pass.pushConstantSize, 0);
            if (!pass.pushDefaults.empty()) {
                size_t srcBytes = pass.pushDefaults.size() * sizeof(float);
                size_t copyBytes = std::min(srcBytes, static_cast<size_t>(pass.pushConstantSize));
                std::memcpy(pushData.data(), pass.pushDefaults.data(), copyBytes);
            }
            renderer_->cmdPushConstants(pass.pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, pass.pushConstantSize, pushData.data());
        }

        if (pass.descriptorSet) {
            renderer_->cmdBindDescriptorSet(pass.pipelineLayout, pass.descriptorSet);
        }

        renderer_->drawFullScreenQuad();

        renderer_->cmdEndRendering();

        if (!isLastPass && pass.renderTarget.image) {
            renderer_->cmdTransitionImage(pass.renderTarget.image,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else if (isLastPass) {
            renderer_->cmdTransitionImage(renderer_->currentSwapchainImage(),
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        }
    }

    renderer_->endFrame();
    renderer_->present();
    ++frameCount_;
    return Status::success();
}

}  // namespace monix::renderer_vk
