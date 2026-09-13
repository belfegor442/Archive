#include "vk/image/VkImage.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"
#include "vk/memory/VkMemory.hpp"

struct VkCtx { VulkanRenderer& r; };

VkImageResource vk_img::createImage(VkCtx& ctx, uint32_t w, uint32_t h, VkFormat fmt,
                                     bool renderTarget, bool cpuReadable) {
    auto& r = ctx.r;
    VkImageResource result{};

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { w, h, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = fmt;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (renderTarget) {
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    } else if (cpuReadable) {
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    } else {
        imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (pfn_vkCreateImage(r.device_, &imageInfo, nullptr, &result.image) != VK_SUCCESS) {
        return result;
    }

    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (cpuReadable) {
        memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    if (!vk_mem::allocateImageMemory(ctx, result.image, memProps, &result.memory)) {
        pfn_vkDestroyImage(r.device_, result.image, nullptr);
        result.image = VK_NULL_HANDLE;
        return result;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = result.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = fmt;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (pfn_vkCreateImageView(r.device_, &viewInfo, nullptr, &result.view) != VK_SUCCESS) {
        pfn_vkDestroyImage(r.device_, result.image, nullptr);
        pfn_vkFreeMemory(r.device_, result.memory, nullptr);
        result.image = VK_NULL_HANDLE;
        return result;
    }

    result.format = fmt;
    result.width = w;
    result.height = h;
    result.isRenderTarget = renderTarget;
    result.currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return result;
}

void vk_img::destroyImage(VkCtx& ctx, VkImageResource& img) {
    auto& r = ctx.r;
    if (img.view) pfn_vkDestroyImageView(r.device_, img.view, nullptr);
    if (img.image) pfn_vkDestroyImage(r.device_, img.image, nullptr);
    if (img.memory) pfn_vkFreeMemory(r.device_, img.memory, nullptr);
    img = {};
}
