#include "vk/preset/VkPreset.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"
#include "vulkan-headers/vulkan_win32.h"
#include "vk/buffer/VkBuffer.hpp"
#include "vk/image/VkImage.hpp"
#include "vk/sampler/VkSampler.hpp"
#include "vk/descriptor/VkDescriptor.hpp"
#include "vk/pipeline/VkPipeline.hpp"
#include "vk/cmd/VkCmdRecording.hpp"
#include "vk/screenshot/VkScreenshot.hpp"

#include <cstring>

struct VkCtx { VulkanRenderer& r; };

static const uint32_t kMinimalVertSpv[] = {
    0x07230203, 0x00010300, 0x00280000, 0x00000017, 0x00000000, 0x00020011, 0x00000001, 0x0003000E, 0x00000000, 0x00000001, 0x0009000F, 0x00000000, 0x00000002, 0x6E69616D, 0x00000000, 0x00000011, 0x00000014, 0x00000009, 0x0000000F, 0x00030003, 0x0000000B, 0x00000001, 0x00050005, 0x00000009, 0x69736F70, 0x6E6F6974, 0x00000000, 0x00050005, 0x0000000F, 0x43786574, 0x64726F6F, 0x00000000, 0x000A0005, 0x00000014, 0x72746E65, 0x696F5079, 0x6150746E, 0x5F6D6172, 0x6E69616D, 0x745F762E, 0x6F437865, 0x0064726F, 0x00040005, 0x00000002, 0x6E69616D, 0x00000000, 0x00040047, 0x00000009, 0x0000001E, 0x00000000, 0x00040047, 0x0000000F, 0x0000001E, 0x00000001, 0x00040047, 0x00000011, 0x0000000B, 0x00000000, 0x00040047, 0x00000014, 0x0000001E, 0x00000000, 0x00020013, 0x00000001, 0x00030021, 0x00000003, 0x00000001, 0x00030016, 0x00000005, 0x00000020, 0x00040017, 0x00000006, 0x00000005, 0x00000002, 0x00040020, 0x00000008, 0x00000001, 0x00000006, 0x00040017, 0x0000000A, 0x00000005, 0x00000004, 0x0004002B, 0x00000005, 0x0000000C, 0x00000000, 0x0004002B, 0x00000005, 0x0000000D, 0x3F800000, 0x00040020, 0x00000010, 0x00000003, 0x0000000A, 0x00040020, 0x00000013, 0x00000003, 0x00000006, 0x0004003B, 0x00000008, 0x00000009, 0x00000001, 0x0004003B, 0x00000008, 0x0000000F, 0x00000001, 0x0004003B, 0x00000010, 0x00000011, 0x00000003, 0x0004003B, 0x00000013, 0x00000014, 0x00000003, 0x00050036, 0x00000001, 0x00000002, 0x00000000, 0x00000003, 0x000200F8, 0x00000004, 0x0004003D, 0x00000006, 0x00000007, 0x00000009, 0x00060050, 0x0000000A, 0x0000000B, 0x00000007, 0x0000000C, 0x0000000D, 0x0004003D, 0x00000006, 0x0000000E, 0x0000000F, 0x0003003E, 0x00000011, 0x0000000B, 0x0003003E, 0x00000014, 0x0000000E, 0x000100FD, 0x00010038
};
static const size_t kMinimalVertSpvWordCount = sizeof(kMinimalVertSpv) / sizeof(kMinimalVertSpv[0]);

bool vk_preset::loadPreset(VkCtx& ctx, uint32_t width, uint32_t height,
    const std::vector<std::vector<uint32_t>>& vertSpvs,
    const std::vector<std::vector<uint32_t>>& fragSpvs,
    const std::vector<uint32_t>& samplerCounts,
    const std::vector<uint32_t>& pushSizes,
    const std::vector<uint32_t>& rtFormats,
    const std::vector<std::vector<float>>& pushDefaultsList,
    const std::vector<uint32_t>& uboSizes) {

    auto& r = ctx.r;
    destroyPreset(ctx);
    r.preset_.outputWidth = width;
    r.preset_.outputHeight = height;

    size_t passCount = vertSpvs.size();
    r.preset_.passes.resize(passCount);

    FILE* logf = nullptr;
    fopen_s(&logf, "build\\vk_preset.log", "w");
    if (logf) setvbuf(logf, nullptr, _IONBF, 0);
    if (logf) fprintf(logf, "loadPreset: %zu passes\n", passCount);

    for (size_t i = 0; i < passCount; i++) {
        auto& pass = r.preset_.passes[i];

        pass.vertSpirv = vertSpvs[i];
        pass.fragSpirv = fragSpvs[i];
        pass.samplerCount = samplerCounts[i];
        pass.samplerBindings.clear();
        for (uint32_t b = 0; b < pass.samplerCount; b++) {
            pass.samplerBindings.push_back(2 + b);
        }
        pass.pushConstantSize = pushSizes[i];
        if (i < pushDefaultsList.size()) pass.pushDefaults = pushDefaultsList[i];
        pass.uboSize = (i < uboSizes.size() && uboSizes[i] > 0) ? uboSizes[i] : sizeof(UniformData);

        if (logf) fprintf(logf, "\nPass %zu: samplers=%u push=%u vert=%zu frag=%zu\n",
            i, pass.samplerCount, pass.pushConstantSize, pass.vertSpirv.size(), pass.fragSpirv.size());

        if (pass.vertSpirv.empty() || pass.vertSpirv[0] != 0x07230203) {
            if (logf) fprintf(logf, "  BAD vert SPIR-V magic: 0x%08X\n", pass.vertSpirv.empty() ? 0 : pass.vertSpirv[0]);
        }
        if (pass.fragSpirv.empty() || pass.fragSpirv[0] != 0x07230203) {
            if (logf) fprintf(logf, "  BAD frag SPIR-V magic: 0x%08X\n", pass.fragSpirv.empty() ? 0 : pass.fragSpirv[0]);
        }

        if (logf) { fprintf(logf, "  creating vert module (%zu words)...\n", pass.vertSpirv.size()); }
        pass.vertModule = vk_pipe::createShaderModule(ctx, pass.vertSpirv);
        if (logf) { fprintf(logf, "  vert module = %p\n", (void*)pass.vertModule); }
        if (logf) { fprintf(logf, "  creating frag module (%zu words)...\n", pass.fragSpirv.size()); }
        pass.fragModule = vk_pipe::createShaderModule(ctx, pass.fragSpirv);
        if (logf) { fprintf(logf, "  frag module = %p\n", (void*)pass.fragModule); }
        if (!pass.vertModule || !pass.fragModule) {
            if (logf) fprintf(logf, "  FAILED: shader modules — marking pass %zu as invalid\n", i);
            pass.valid = false;
            continue;
        }
        pass.valid = true;
        if (logf) fprintf(logf, "  shader modules OK\n");

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(uboBinding);

        for (uint32_t b = 0; b < pass.samplerCount; b++) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 2 + b;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(binding);
        }
        if (!bindings.empty()) {
            pass.descSetLayout = vk_desc::createDescriptorSetLayout(ctx, bindings);
        }

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = pass.pushConstantSize > 0 ? pass.pushConstantSize : 64;

        std::vector<VkDescriptorSetLayout> layouts;
        if (pass.descSetLayout) layouts.push_back(pass.descSetLayout);
        std::vector<VkPushConstantRange> pushRanges = {pushRange};
        pass.pipelineLayout = vk_pipe::createPipelineLayout(ctx, layouts, pushRanges);

        VkFormat rtFormat = (i < rtFormats.size()) ? static_cast<VkFormat>(rtFormats[i]) : r.swapchainFormat_;
        VkVertexInputBindingDescription vertBinding{};
        vertBinding.binding = 0;
        vertBinding.stride = sizeof(QuadVertex);
        vertBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertAttrs[2]{};
        vertAttrs[0].binding = 0;
        vertAttrs[0].location = 0;
        vertAttrs[0].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[0].offset = offsetof(QuadVertex, position);
        vertAttrs[1].binding = 0;
        vertAttrs[1].location = 1;
        vertAttrs[1].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[1].offset = offsetof(QuadVertex, texCoord);

        pass.pipeline = vk_pipe::createGraphicsPipeline(ctx,
            pass.vertSpirv, pass.fragSpirv,
            pass.pipelineLayout, rtFormat,
            VK_FORMAT_UNDEFINED, false,
            vertBinding, vertAttrs);

        if (!pass.pipeline) {
            if (logf) fprintf(logf, "  FAILED: pipeline creation for pass %zu — marking invalid\n", i);
            pass.valid = false;
            continue;
        }

        if (logf) fprintf(logf, "  pipeline OK\n");

        pass.width = width;
        pass.height = height;
        pass.renderTarget = vk_img::createImage(ctx, width, height, rtFormat, true, false);

        pass.uniformBuffer = vk_buf::createBuffer(ctx,
            pass.uboSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (pass.descSetLayout) {
            pass.descriptorSet = vk_desc::allocateDescriptorSet(ctx, pass.descSetLayout);
            if (pass.descriptorSet && pass.uniformBuffer.buffer) {
                vk_desc::updateDescriptorSetUniform(ctx, pass.descriptorSet, 0,
                    pass.uniformBuffer.buffer, pass.uboSize);
            }
        }
    }

    auto linearSampler = vk_smp::createSampler(ctx,
        VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0, 0, 0, 1);
    if (linearSampler.sampler) {
        r.preset_.samplers.push_back(linearSampler);
    }

    if (logf) { fprintf(logf, "\nloadPreset: SUCCESS (%zu samplers)\n", r.preset_.samplers.size()); fclose(logf); }
    return true;
}

bool vk_preset::loadPresetReflection(VkCtx& ctx, uint32_t width, uint32_t height,
    const std::vector<std::vector<uint32_t>>& vertSpvs,
    const std::vector<std::vector<uint32_t>>& fragSpvs,
    const std::vector<uint32_t>& pushSizes,
    const std::vector<uint32_t>& rtFormats,
    const std::vector<std::vector<float>>& pushDefaultsList,
    const std::vector<uint32_t>& uboSizes,
    const std::vector<std::vector<PresetSamplerBinding>>& perPassSamplerBindings) {

    auto& r = ctx.r;
    destroyPreset(ctx);
    r.preset_.outputWidth = width;
    r.preset_.outputHeight = height;

    size_t passCount = vertSpvs.size();
    r.preset_.passes.resize(passCount);

    FILE* logf = nullptr;
    fopen_s(&logf, "build\\vk_preset_reflect.log", "w");
    if (logf) fprintf(logf, "loadPresetReflection: %zu passes\n", passCount);

    for (size_t i = 0; i < passCount; i++) {
        auto& pass = r.preset_.passes[i];

        pass.vertSpirv = vertSpvs[i];
        pass.fragSpirv = fragSpvs[i];
        pass.pushConstantSize = pushSizes[i];
        if (i < pushDefaultsList.size()) pass.pushDefaults = pushDefaultsList[i];
        pass.uboSize = (i < uboSizes.size() && uboSizes[i] > 0) ? uboSizes[i] : sizeof(UniformData);

        const auto& samplerBindings = (i < perPassSamplerBindings.size()) ? perPassSamplerBindings[i] : std::vector<PresetSamplerBinding>{};
        pass.samplerCount = static_cast<uint32_t>(samplerBindings.size());
        pass.samplerBindings.clear();
        pass.samplerNames.clear();
        for (const auto& sb : samplerBindings) {
            pass.samplerBindings.push_back(sb.binding);
            pass.samplerNames.push_back(sb.samplerName);
        }

        if (logf) fprintf(logf, "\nPass %zu: samplers=%u push=%u vert=%zu frag=%zu\n",
            i, pass.samplerCount, pass.pushConstantSize, pass.vertSpirv.size(), pass.fragSpirv.size());

        pass.vertModule = vk_pipe::createShaderModule(ctx, pass.vertSpirv);
        pass.fragModule = vk_pipe::createShaderModule(ctx, pass.fragSpirv);
        if (!pass.vertModule || !pass.fragModule) {
            if (logf) fprintf(logf, "  FAILED: shader modules\n");
            if (logf) fclose(logf);
            destroyPreset(ctx);
            return false;
        }
        if (logf) fprintf(logf, "  shader modules OK\n");

        std::vector<VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(uboBinding);

        for (const auto& sb : samplerBindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = sb.binding;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(binding);
            if (logf) fprintf(logf, "  sampler binding=%u name=%s\n", sb.binding, sb.samplerName.c_str());
        }
        if (!bindings.empty()) {
            pass.descSetLayout = vk_desc::createDescriptorSetLayout(ctx, bindings);
        }

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = pass.pushConstantSize > 0 ? pass.pushConstantSize : 64;

        std::vector<VkDescriptorSetLayout> layouts;
        if (pass.descSetLayout) layouts.push_back(pass.descSetLayout);
        std::vector<VkPushConstantRange> pushRanges = {pushRange};
        pass.pipelineLayout = vk_pipe::createPipelineLayout(ctx, layouts, pushRanges);

        VkFormat rtFormat = (i < rtFormats.size()) ? static_cast<VkFormat>(rtFormats[i]) : r.swapchainFormat_;
        VkVertexInputBindingDescription vertBinding{};
        vertBinding.binding = 0;
        vertBinding.stride = sizeof(QuadVertex);
        vertBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertAttrs[2]{};
        vertAttrs[0].binding = 0;
        vertAttrs[0].location = 0;
        vertAttrs[0].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[0].offset = offsetof(QuadVertex, position);
        vertAttrs[1].binding = 0;
        vertAttrs[1].location = 1;
        vertAttrs[1].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[1].offset = offsetof(QuadVertex, texCoord);

        pass.pipeline = vk_pipe::createGraphicsPipeline(ctx,
            pass.vertSpirv, pass.fragSpirv,
            pass.pipelineLayout, rtFormat,
            VK_FORMAT_UNDEFINED, false,
            vertBinding, vertAttrs);

        if (!pass.pipeline) {
            if (logf) fprintf(logf, "  FAILED: pipeline creation\n");
            if (logf) fclose(logf);
            destroyPreset(ctx);
            return false;
        }

        if (logf) fprintf(logf, "  pipeline OK\n");

        pass.width = width;
        pass.height = height;
        pass.renderTarget = vk_img::createImage(ctx, width, height, rtFormat, true, false);

        pass.uniformBuffer = vk_buf::createBuffer(ctx,
            pass.uboSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (pass.descSetLayout) {
            pass.descriptorSet = vk_desc::allocateDescriptorSet(ctx, pass.descSetLayout);
            if (pass.descriptorSet && pass.uniformBuffer.buffer) {
                vk_desc::updateDescriptorSetUniform(ctx, pass.descriptorSet, 0,
                    pass.uniformBuffer.buffer, pass.uboSize);
            }
        }
    }

    auto linearSampler = vk_smp::createSampler(ctx,
        VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0, 0, 0, 1);
    if (linearSampler.sampler) {
        r.preset_.samplers.push_back(linearSampler);
    }

    if (logf) { fprintf(logf, "\nloadPresetReflection: SUCCESS (%zu samplers)\n", r.preset_.samplers.size()); fclose(logf); }
    return true;
}

void vk_preset::destroyPreset(VkCtx& ctx) {
    auto& r = ctx.r;
    for (auto& pass : r.preset_.passes) {
        if (pass.pipeline) vk_pipe::destroyPipeline(ctx, pass.pipeline);
        if (pass.pipelineLayout) vk_pipe::destroyPipelineLayout(ctx, pass.pipelineLayout);
        if (pass.descSetLayout) vk_desc::destroyDescriptorSetLayout(ctx, pass.descSetLayout);
        if (pass.vertModule) vk_pipe::destroyShaderModule(ctx, pass.vertModule);
        if (pass.fragModule) vk_pipe::destroyShaderModule(ctx, pass.fragModule);
        if (pass.renderTarget.image) vk_img::destroyImage(ctx, pass.renderTarget);
        if (pass.uniformBuffer.buffer) vk_buf::destroyBuffer(ctx, pass.uniformBuffer);
    }
    for (auto& s : r.preset_.samplers) {
        vk_smp::destroySampler(ctx, s);
    }
    for (auto& img : r.preset_.images) {
        vk_img::destroyImage(ctx, img);
    }
    r.preset_ = {};
}

bool vk_preset::renderPreset(VkCtx& ctx, uint32_t width, uint32_t height) {
    auto& r = ctx.r;
    if (!r.preset_.valid()) { OutputDebugStringA("[VK] renderPreset FAIL: preset not valid\n"); return false; }
    if (!r.frameActive_) { OutputDebugStringA("[VK] renderPreset FAIL: frame not active\n"); return false; }

    VkImageView sourceView = r.uploadImage_.view;
    if (!sourceView) { OutputDebugStringA("[VK] renderPreset FAIL: sourceView null\n"); return false; }

    char buf[256];
    sprintf_s(buf, "[VK] renderPreset: %zu passes, src=%dx%d\n", r.preset_.passes.size(), r.uploadImage_.width, r.uploadImage_.height);
    OutputDebugStringA(buf);

    for (size_t i = 0; i < r.preset_.passes.size(); i++) {
        auto& pass = r.preset_.passes[i];

        if (!pass.valid || !pass.pipeline) {
            OutputDebugStringA("[VK] renderPreset: skipping invalid pass\n");
            continue;
        }

        bool isLastPass = (i == r.preset_.passes.size() - 1);
        VkImageView rtView = isLastPass ? r.currentSwapchainView() : pass.renderTarget.view;
        uint32_t rtW = isLastPass ? width : pass.width;
        uint32_t rtH = isLastPass ? height : pass.height;

        if (isLastPass) {
            vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        } else if (pass.renderTarget.image) {
            vk_rec::cmdTransitionImage(ctx, pass.renderTarget.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        if (pass.descriptorSet) {
            for (size_t s = 0; s < pass.samplerCount && s < pass.samplerBindings.size(); s++) {
                uint32_t binding = pass.samplerBindings[s];
                const std::string& name = (s < pass.samplerNames.size()) ? pass.samplerNames[s] : "";
                VkImageView imageView = VK_NULL_HANDLE;

                if (name == "Source") {
                    imageView = (i == 0) ? sourceView : r.preset_.passes[i - 1].renderTarget.view;
                } else if (name == "PastSampler") {
                    imageView = (i > 0) ? r.preset_.passes[i - 1].renderTarget.view : sourceView;
                } else {
                    for (size_t p = 0; p < r.preset_.passes.size(); p++) {
                        if (p == i) continue;
                        const auto& otherPass = r.preset_.passes[p];
                        if (!otherPass.renderTarget.view) continue;
                        if (!name.empty() && otherPass.renderTarget.view) {
                            bool matched = false;
                            for (size_t q = 0; q < p && q < r.preset_.passes.size(); q++) {
                                const auto& earlierPass = r.preset_.passes[q];
                                for (const auto& eName : earlierPass.samplerNames) {
                                    if (eName == name) { matched = true; break; }
                                }
                                if (matched) break;
                            }
                            imageView = otherPass.renderTarget.view;
                            matched = true;
                            break;
                        }
                    }
                }

                if (!imageView) {
                    imageView = sourceView;
                }

                if (imageView) {
                    vk_desc::updateDescriptorSetTexture(ctx, pass.descriptorSet, binding,
                        imageView, r.preset_.samplers[0].sampler,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
            }
        }

        VkClearValue clear{};
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        vk_rec::cmdBeginRendering(ctx, rtView, rtW, rtH, clear);

        vk_rec::cmdBindPipeline(ctx, pass.pipeline, pass.pipelineLayout);

        vk_rec::cmdSetViewport(ctx, rtW, rtH);
        vk_rec::cmdSetScissor(ctx, rtW, rtH);

        struct PresetUBO {
            float mvp[16];
            float outputSize[4];
            float originalSize[4];
            float sourceSize[4];
        } ubo{};
        ubo.mvp[0] =  1.0f;
        ubo.mvp[5] =  1.0f;
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
        float srcW, srcH;
        if (i == 0) {
            srcW = static_cast<float>(r.uploadImage_.width);
            srcH = static_cast<float>(r.uploadImage_.height);
        } else {
            srcW = static_cast<float>(rtW);
            srcH = static_cast<float>(rtH);
        }
        ubo.sourceSize[0] = srcW;
        ubo.sourceSize[1] = srcH;
        ubo.sourceSize[2] = 1.0f / srcW;
        ubo.sourceSize[3] = 1.0f / srcH;

        vk_buf::uploadBuffer(ctx, pass.uniformBuffer, &ubo, sizeof(ubo));

        std::vector<uint8_t> pushData(pass.pushConstantSize > 0 ? pass.pushConstantSize : 0, 0);
        if (!pass.pushDefaults.empty() && pass.pushConstantSize > 0) {
            size_t srcBytes = pass.pushDefaults.size() * sizeof(float);
            size_t copyBytes = (srcBytes < static_cast<size_t>(pass.pushConstantSize))
                             ? srcBytes : static_cast<size_t>(pass.pushConstantSize);
            memcpy(pushData.data(), pass.pushDefaults.data(), copyBytes);
        }
        if (!pushData.empty()) {
            vk_rec::cmdPushConstants(ctx, pass.pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, pass.pushConstantSize, pushData.data());
        }

        if (pass.descriptorSet) {
            vk_rec::cmdBindDescriptorSet(ctx, pass.pipelineLayout, pass.descriptorSet, 0);
        }

        vk_rec::drawFullScreenQuad(ctx);

        vk_rec::cmdEndRendering(ctx);

        if (!isLastPass && pass.renderTarget.image) {
            vk_rec::cmdTransitionImage(ctx, pass.renderTarget.image,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else if (isLastPass) {
            if (r.screenshot_.pending && r.screenshot_.width > 0 && r.screenshot_.height > 0 && !r.screenshotCopied_) {
                vk_ss::ensureScreenshotStaging(ctx, r.screenshot_.width, r.screenshot_.height);
                vk_ss::executeScreenshotCopy(ctx, r.screenshot_.width, r.screenshot_.height);
            } else {
                vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            }
        }
    }

    return true;
}

bool vk_preset::blitAppContentOverPreset(VkCtx& ctx, uint32_t width, uint32_t height) {
    auto& r = ctx.r;
    if (!r.frameActive_) return false;
    if (!r.uploadImage_.image) return false;

    vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_rec::cmdTransitionLayout(ctx, r.uploadImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageBlit region{};
    region.srcOffsets[0] = { 0, 0, 0 };
    region.srcOffsets[1] = { static_cast<int32_t>(r.uploadImage_.width),
                             static_cast<int32_t>(r.uploadImage_.height), 1 };
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstOffsets[0] = { 0, 0, 0 };
    region.dstOffsets[1] = { static_cast<int32_t>(r.swapchainExtent_.width),
                             static_cast<int32_t>(r.swapchainExtent_.height), 1 };
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;

    pfn_vkCmdBlitImage(r.cmdBuffers_[r.currentFrame_],
        r.uploadImage_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        r.currentSwapchainImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, VK_FILTER_LINEAR);

    vk_rec::cmdTransitionLayout(ctx, r.uploadImage_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    return true;
}
