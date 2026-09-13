#pragma once

#include <cstdint>
#include <span>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;

namespace vk_desc {
bool createDescriptorPool(VkCtx& ctx);
VkDescriptorSetLayout createDescriptorSetLayout(VkCtx& ctx, std::span<const VkDescriptorSetLayoutBinding> bindings);
void destroyDescriptorSetLayout(VkCtx& ctx, VkDescriptorSetLayout layout);
VkDescriptorSet allocateDescriptorSet(VkCtx& ctx, VkDescriptorSetLayout layout);
void updateDescriptorSetTexture(VkCtx& ctx, VkDescriptorSet set, uint32_t binding,
                                 VkImageView view, VkSampler sampler, VkImageLayout layout);
void updateDescriptorSetUniform(VkCtx& ctx, VkDescriptorSet set, uint32_t binding,
                                 VkBuffer buffer, VkDeviceSize range);
}
