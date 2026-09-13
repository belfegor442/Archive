#pragma once

#include <cstdint>
#include <span>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;

namespace vk_pipe {
VkPipelineLayout createPipelineLayout(VkCtx& ctx, std::span<const VkDescriptorSetLayout> descLayouts,
                                       std::span<const VkPushConstantRange> pushRanges);
void destroyPipelineLayout(VkCtx& ctx, VkPipelineLayout layout);
VkPipeline createGraphicsPipeline(VkCtx& ctx, std::span<const uint32_t> vertSpirv,
                                   std::span<const uint32_t> fragSpirv, VkPipelineLayout layout,
                                   VkFormat colorFormat, VkFormat depthFormat, bool hasBlend,
                                   VkVertexInputBindingDescription vertBinding,
                                   std::span<const VkVertexInputAttributeDescription> vertAttrs);
void destroyPipeline(VkCtx& ctx, VkPipeline pipeline);
VkShaderModule createShaderModule(VkCtx& ctx, std::span<const uint32_t> spirv);
void destroyShaderModule(VkCtx& ctx, VkShaderModule mod);
}
