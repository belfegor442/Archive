#pragma once

#include <cstdint>

struct VkCtx;

namespace vk_ss {
void requestScreenshot(VkCtx& ctx, uint32_t w, uint32_t h);
void ensureScreenshotStaging(VkCtx& ctx, uint32_t w, uint32_t h);
void executeScreenshotCopy(VkCtx& ctx, uint32_t w, uint32_t h);
void finalizeScreenshot(VkCtx& ctx);
}
