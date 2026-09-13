#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;
struct VkBufferResource;

namespace vk_buf {
VkBufferResource createBuffer(VkCtx& ctx, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags props);
void destroyBuffer(VkCtx& ctx, VkBufferResource& buf);
void uploadBuffer(VkCtx& ctx, VkBufferResource& dst, const void* data, VkDeviceSize size);
bool createQuadBuffer(VkCtx& ctx);
}
