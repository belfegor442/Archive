#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;
struct VkImageResource;

namespace vk_cmd {
bool createCommandPool(VkCtx& ctx);
bool allocateCommandBuffers(VkCtx& ctx);
bool transitionImageLayoutImmediate(VkCtx& ctx, VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout);
}
