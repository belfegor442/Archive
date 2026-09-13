#include "vk/memory/VkMemory.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"

struct VkCtx { VulkanRenderer& r; };

uint32_t vk_mem::findMemoryType(VkCtx& ctx, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    auto& r = ctx.r;
    VkPhysicalDeviceMemoryProperties memProps{};
    pfn_vkGetPhysicalDeviceMemoryProperties(r.physDev_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    OutputDebugStringA("[VK] Failed to find suitable memory type\n");
    return UINT32_MAX;
}

bool vk_mem::allocateImageMemory(VkCtx& ctx, VkImage image, VkMemoryPropertyFlags props, VkDeviceMemory* outMem) {
    auto& r = ctx.r;
    VkMemoryRequirements memReqs{};
    pfn_vkGetImageMemoryRequirements(r.device_, image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(ctx, memReqs.memoryTypeBits, props);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) return false;

    VkResult result = pfn_vkAllocateMemory(r.device_, &allocInfo, nullptr, outMem);
    if (result != VK_SUCCESS) return false;

    result = pfn_vkBindImageMemory(r.device_, image, *outMem, 0);
    if (result != VK_SUCCESS) {
        pfn_vkFreeMemory(r.device_, *outMem, nullptr);
        *outMem = VK_NULL_HANDLE;
        return false;
    }
    return true;
}
