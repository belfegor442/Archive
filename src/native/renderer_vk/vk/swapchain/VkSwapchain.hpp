#pragma once

#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

struct VkCtx;

namespace vk_swap {
bool createSurface(VkCtx& ctx, HWND hwnd);
bool createSwapchain(VkCtx& ctx, uint32_t width, uint32_t height);
void destroySwapchain(VkCtx& ctx);
void resize(VkCtx& ctx, uint32_t width, uint32_t height);
}
