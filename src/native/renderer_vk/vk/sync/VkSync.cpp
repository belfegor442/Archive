#include "vk/sync/VkSync.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"

struct VkCtx { VulkanRenderer& r; };

bool vk_sync::createSyncObjects(VkCtx& ctx) {
    auto& r = ctx.r;
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
        if (pfn_vkCreateSemaphore(r.device_, &semInfo, nullptr, &r.imageAvailableSem_[i]) != VK_SUCCESS ||
            pfn_vkCreateSemaphore(r.device_, &semInfo, nullptr, &r.renderFinishedSem_[i]) != VK_SUCCESS ||
            pfn_vkCreateFence(r.device_, &fenceInfo, nullptr, &r.inFlightFences_[i]) != VK_SUCCESS) {
            OutputDebugStringA("[VK] Failed to create sync objects\n");
            for (uint32_t j = 0; j <= i; j++) {
                if (r.imageAvailableSem_[j]) { pfn_vkDestroySemaphore(r.device_, r.imageAvailableSem_[j], nullptr); r.imageAvailableSem_[j] = VK_NULL_HANDLE; }
                if (r.renderFinishedSem_[j]) { pfn_vkDestroySemaphore(r.device_, r.renderFinishedSem_[j], nullptr); r.renderFinishedSem_[j] = VK_NULL_HANDLE; }
                if (r.inFlightFences_[j]) { pfn_vkDestroyFence(r.device_, r.inFlightFences_[j], nullptr); r.inFlightFences_[j] = VK_NULL_HANDLE; }
            }
            return false;
        }
    }

    return true;
}
