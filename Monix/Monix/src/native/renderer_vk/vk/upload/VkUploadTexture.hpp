#pragma once

#include <cstdint>

struct VkCtx;

namespace vk_upl {
bool uploadTexture(VkCtx& ctx, const void* bgraPixels, uint32_t w, uint32_t h);
bool uploadTextureToImage(VkCtx& ctx, const void* bgraPixels, uint32_t w, uint32_t h, bool blitToSwapchain);
}
