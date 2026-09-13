#include "vk/sampler/VkSampler.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"

struct VkCtx { VulkanRenderer& r; };

VkSamplerResource vk_smp::createSampler(VkCtx& ctx, VkFilter minFilter, VkFilter magFilter,
                                          VkSamplerAddressMode wrapS, VkSamplerAddressMode wrapT,
                                          float borderColorR, float borderColorG,
                                          float borderColorB, float borderColorA) {
    auto& r = ctx.r;
    VkSamplerResource result{};

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
    samplerInfo.addressModeU = wrapS;
    samplerInfo.addressModeV = wrapT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    if (wrapS == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ||
        wrapT == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER) {
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    }

    if (pfn_vkCreateSampler(r.device_, &samplerInfo, nullptr, &result.sampler) != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create sampler\n");
    }
    return result;
}

void vk_smp::destroySampler(VkCtx& ctx, VkSamplerResource& s) {
    auto& r = ctx.r;
    if (s.sampler) pfn_vkDestroySampler(r.device_, s.sampler, nullptr);
    s = {};
}
