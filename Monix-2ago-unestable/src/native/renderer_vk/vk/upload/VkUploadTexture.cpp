#include "vk/upload/VkUploadTexture.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"
#include "vk/buffer/VkBuffer.hpp"
#include "vk/image/VkImage.hpp"
#include "vk/cmd/VkCmdRecording.hpp"

struct VkCtx { VulkanRenderer& r; };

bool vk_upl::uploadTexture(VkCtx& ctx, const void* bgraPixels, uint32_t w, uint32_t h) {
    return uploadTextureToImage(ctx, bgraPixels, w, h, true);
}

bool vk_upl::uploadTextureToImage(VkCtx& ctx, const void* bgraPixels, uint32_t w, uint32_t h, bool blitToSwapchain) {
    auto& r = ctx.r;
    if (!r.frameActive_ || !bgraPixels || w == 0 || h == 0) return false;

    VkDeviceSize dataSize = static_cast<VkDeviceSize>(w) * h * 4;
    if (!r.uploadCreated_ || r.uploadStaging_.size < dataSize) {
        if (r.uploadCreated_) {
            vk_buf::destroyBuffer(ctx, r.uploadStaging_);
            vk_img::destroyImage(ctx, r.uploadImage_);
        }
        r.uploadStaging_ = vk_buf::createBuffer(ctx, dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        r.uploadImage_ = vk_img::createImage(ctx, w, h, VK_FORMAT_B8G8R8A8_UNORM, false, false);
        if (!r.uploadStaging_.buffer || !r.uploadImage_.image) {
            return false;
        }
        r.uploadCreated_ = true;
    } else if (r.uploadImage_.width != w || r.uploadImage_.height != h) {
        vk_img::destroyImage(ctx, r.uploadImage_);
        r.uploadImage_ = vk_img::createImage(ctx, w, h, VK_FORMAT_B8G8R8A8_UNORM, false, false);
        if (!r.uploadImage_.image) return false;
    }

    vk_buf::uploadBuffer(ctx, r.uploadStaging_, bgraPixels, dataSize);

    vk_rec::cmdTransitionLayout(ctx, r.uploadImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vk_rec::cmdCopyBufferToImage(ctx, r.uploadStaging_, r.uploadImage_, w, h);

    if (blitToSwapchain) {
        vk_rec::cmdTransitionLayout(ctx, r.uploadImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageBlit region{};
        region.srcOffsets[0] = { 0, static_cast<int32_t>(h), 0 };
        region.srcOffsets[1] = { static_cast<int32_t>(w), 0, 1 };
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstOffsets[0] = { 0, 0, 0 };
        region.dstOffsets[1] = { static_cast<int32_t>(r.swapchainExtent_.width),
                                  static_cast<int32_t>(r.swapchainExtent_.height), 1 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;

        pfn_vkCmdBlitImage(r.cmdBuffers_[r.currentFrame_],
            r.uploadImage_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            r.currentSwapchainImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region, VK_FILTER_LINEAR);

        vk_rec::cmdTransitionImage(ctx, r.currentSwapchainImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } else {
        vk_rec::cmdTransitionLayout(ctx, r.uploadImage_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    return true;
}
