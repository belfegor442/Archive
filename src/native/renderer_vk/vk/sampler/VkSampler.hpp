#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;
struct VkSamplerResource;

namespace vk_smp {
VkSamplerResource createSampler(VkCtx& ctx, VkFilter minFilter, VkFilter magFilter,
                                 VkSamplerAddressMode wrapS, VkSamplerAddressMode wrapT,
                                 float borderColorR, float borderColorG,
                                 float borderColorB, float borderColorA);
void destroySampler(VkCtx& ctx, VkSamplerResource& s);
}
