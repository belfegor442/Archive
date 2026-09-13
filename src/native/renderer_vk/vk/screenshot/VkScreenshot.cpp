#include "vk/screenshot/VkScreenshot.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"
#include "vk/buffer/VkBuffer.hpp"
#include "vk/cmd/VkCmdRecording.hpp"
#include <cstring>

struct VkCtx { VulkanRenderer& r; };

void vk_ss::requestScreenshot(VkCtx& ctx, uint32_t w, uint32_t h) {
    auto& r = ctx.r;
    r.screenshot_.pending = true;
    r.screenshot_.width = w;
    r.screenshot_.height = h;
    r.screenshot_.pixels.resize(w * h * 4);
    r.screenshotCopied_ = false;
}

void vk_ss::ensureScreenshotStaging(VkCtx& ctx, uint32_t w, uint32_t h) {
    auto& r = ctx.r;
    VkDeviceSize needed = static_cast<VkDeviceSize>(w) * h * 4;
    if (r.screenshotStagingCreated_ && r.screenshotStaging_.size >= needed) return;

    if (r.screenshotStagingCreated_) {
        vk_buf::destroyBuffer(ctx, r.screenshotStaging_);
        r.screenshotStagingCreated_ = false;
    }

    r.screenshotStaging_ = vk_buf::createBuffer(ctx,
        needed,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (r.screenshotStaging_.buffer) {
        r.screenshotStagingCreated_ = true;
    }
}

void vk_ss::executeScreenshotCopy(VkCtx& ctx, uint32_t w, uint32_t h) {
    auto& r = ctx.r;
    if (!r.screenshotStagingCreated_ || !r.screenshotStaging_.buffer) return;
    if (r.screenshotCopied_) return;

    vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImageResource swapchainRes{};
    swapchainRes.image = r.currentSwapchainImage();
    vk_rec::cmdCopyImageToBuffer(ctx, swapchainRes, r.screenshotStaging_, w, h);

    vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    r.screenshotCopied_ = true;
}

void vk_ss::finalizeScreenshot(VkCtx& ctx) {
    auto& r = ctx.r;
    if (!r.screenshot_.pending) return;
    if (!r.screenshotStagingCreated_ || !r.screenshotStaging_.buffer) {
        r.screenshot_.pending = false;
        return;
    }

    r.waitForIdle();

    VkDeviceSize bytes = static_cast<VkDeviceSize>(r.screenshot_.width) * r.screenshot_.height * 4;
    if (r.screenshotStaging_.mapped && r.screenshot_.pixels.size() == bytes) {
        memcpy(r.screenshot_.pixels.data(), r.screenshotStaging_.mapped, static_cast<size_t>(bytes));
    }

    r.screenshot_.pending = false;
}
