#include "vk/device/VkDevice.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"
#include <cstring>
#include <vector>

struct VkCtx { VulkanRenderer& r; };

bool vk_dev::createLogicalDevice(VkCtx& ctx) {
    auto& r = ctx.r;
    float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::vector<uint32_t> uniqueFamilies;
    if (r.graphicsFamily_ == r.presentFamily_) {
        uniqueFamilies.push_back(r.graphicsFamily_);
    } else {
        uniqueFamilies.push_back(r.graphicsFamily_);
        uniqueFamilies.push_back(r.presentFamily_);
    }

    for (auto family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount = 1;
        qi.pQueuePriorities = &queuePriority;
        queueInfos.push_back(qi);
    }

    memset(&r.vk13Features_, 0, sizeof(r.vk13Features_));
    r.vk13Features_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    r.vk13Features_.dynamicRendering = VK_TRUE;
    r.vk13Features_.synchronization2 = VK_TRUE;

    memset(&r.dynamicRenderingFeat_, 0, sizeof(r.dynamicRenderingFeat_));
    r.dynamicRenderingFeat_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    r.dynamicRenderingFeat_.pNext = &r.vk13Features_;
    r.dynamicRenderingFeat_.dynamicRendering = VK_TRUE;

    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &r.dynamicRenderingFeat_;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = extensions;

    VkResult result = pfn_vkCreateDevice(r.physDev_, &createInfo, nullptr, &r.device_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create VkDevice\n");
        return false;
    }

    return true;
}

void vk_dev::loadDeviceFuncs(VkCtx& ctx) {
    auto& r = ctx.r;
    if (!r.device_) return;
#define LOAD(fn) if (!pfn_##fn) pfn_##fn = reinterpret_cast<PFN_##fn>(pfn_vkGetDeviceProcAddr(r.device_, #fn));
    LOAD(vkDestroyDevice)
    LOAD(vkGetDeviceQueue)
    LOAD(vkCreateSwapchainKHR)
    LOAD(vkDestroySwapchainKHR)
    LOAD(vkGetSwapchainImagesKHR)
    LOAD(vkAcquireNextImageKHR)
    LOAD(vkQueuePresentKHR)
    LOAD(vkQueueWaitIdle)
    LOAD(vkDeviceWaitIdle)
    LOAD(vkCreateCommandPool)
    LOAD(vkDestroyCommandPool)
    LOAD(vkAllocateCommandBuffers)
    LOAD(vkFreeCommandBuffers)
    LOAD(vkBeginCommandBuffer)
    LOAD(vkEndCommandBuffer)
    LOAD(vkCmdBeginRendering)
    LOAD(vkCmdEndRendering)
    LOAD(vkCmdBindPipeline)
    LOAD(vkCmdSetViewport)
    LOAD(vkCmdSetScissor)
    LOAD(vkCmdDraw)
    LOAD(vkCmdBlitImage)
    LOAD(vkCmdCopyImageToBuffer)
    LOAD(vkCmdPipelineBarrier)
    LOAD(vkCmdBindVertexBuffers)
    LOAD(vkCmdPushConstants)
    LOAD(vkCmdBindDescriptorSets)
    LOAD(vkCreateFence)
    LOAD(vkDestroyFence)
    LOAD(vkWaitForFences)
    LOAD(vkResetFences)
    LOAD(vkCreateSemaphore)
    LOAD(vkDestroySemaphore)
    LOAD(vkCreateImage)
    LOAD(vkDestroyImage)
    LOAD(vkGetImageMemoryRequirements)
    LOAD(vkAllocateMemory)
    LOAD(vkFreeMemory)
    LOAD(vkBindImageMemory)
    LOAD(vkCreateImageView)
    LOAD(vkDestroyImageView)
    LOAD(vkCreateSampler)
    LOAD(vkDestroySampler)
    LOAD(vkCreateBuffer)
    LOAD(vkDestroyBuffer)
    LOAD(vkGetBufferMemoryRequirements)
    LOAD(vkBindBufferMemory)
    LOAD(vkMapMemory)
    LOAD(vkUnmapMemory)
    LOAD(vkCreateShaderModule)
    LOAD(vkDestroyShaderModule)
    LOAD(vkCreatePipelineLayout)
    LOAD(vkDestroyPipelineLayout)
    LOAD(vkCreateGraphicsPipelines)
    LOAD(vkDestroyPipeline)
    LOAD(vkCreateDescriptorSetLayout)
    LOAD(vkDestroyDescriptorSetLayout)
    LOAD(vkCreateDescriptorPool)
    LOAD(vkDestroyDescriptorPool)
    LOAD(vkAllocateDescriptorSets)
    LOAD(vkUpdateDescriptorSets)
    LOAD(vkQueueSubmit)
    LOAD(vkCmdCopyBufferToImage)
    LOAD(vkGetImageSubresourceLayout)
    LOAD(vkFlushMappedMemoryRanges)
#undef LOAD
}
