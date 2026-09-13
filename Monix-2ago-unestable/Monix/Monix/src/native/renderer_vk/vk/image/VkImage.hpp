#pragma once

#include <cstdint>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;
struct VkImageResource;

namespace vk_img {
VkImageResource createImage(VkCtx& ctx, uint32_t w, uint32_t h, VkFormat fmt, bool renderTarget, bool cpuReadable);
void destroyImage(VkCtx& ctx, VkImageResource& img);
}
