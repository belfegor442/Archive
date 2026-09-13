#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;

namespace vk_mem {
uint32_t findMemoryType(VkCtx& ctx, uint32_t typeFilter, VkMemoryPropertyFlags props);
bool allocateImageMemory(VkCtx& ctx, VkImage image, VkMemoryPropertyFlags props, VkDeviceMemory* outMem);
}
