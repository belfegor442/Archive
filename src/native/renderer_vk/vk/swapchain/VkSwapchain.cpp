#include "vk/swapchain/VkSwapchain.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"
#include <algorithm>
#include <vector>

struct VkCtx { VulkanRenderer& r; };

bool vk_swap::createSurface(VkCtx& ctx, HWND hwnd) {
    auto& r = ctx.r;
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandleA(nullptr);
    createInfo.hwnd = hwnd;

    VkResult result = pfn_vkCreateWin32SurfaceKHR(r.instance_, &createInfo, nullptr, &r.surface_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create Win32 surface\n");
        return false;
    }
    return true;
}

bool vk_swap::createSwapchain(VkCtx& ctx, uint32_t width, uint32_t height) {
    auto& r = ctx.r;

    VkSurfaceCapabilitiesKHR caps{};
    pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(r.physDev_, r.surface_, &caps);

    uint32_t formatCount = 0;
    if (pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(r.physDev_, r.surface_, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) {
        OutputDebugStringA("[VK] No surface formats available\n");
        return false;
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (pfn_vkGetPhysicalDeviceSurfaceFormatsKHR(r.physDev_, r.surface_, &formatCount, formats.data()) != VK_SUCCESS || formatCount == 0) {
        OutputDebugStringA("[VK] Failed to enumerate surface formats\n");
        return false;
    }

    uint32_t presentModeCount = 0;
    if (pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(r.physDev_, r.surface_, &presentModeCount, nullptr) != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to enumerate present modes\n");
        return false;
    }
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0 && pfn_vkGetPhysicalDeviceSurfacePresentModesKHR(r.physDev_, r.surface_, &presentModeCount, presentModes.data()) != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to read present modes\n");
        return false;
    }

    VkSurfaceFormatKHR chosenFormat = formats[0];
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosenFormat = f;
            break;
        }
    }
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM) {
            chosenFormat = f;
            break;
        }
    }

    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : presentModes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = width;
        extent.height = height;
    }
    extent.width = std::clamp(extent.width, caps.minImageExtent.width, caps.maxImageExtent.width);
    extent.height = std::clamp(extent.height, caps.minImageExtent.height, caps.maxImageExtent.height);

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount) {
        imageCount = caps.maxImageCount;
    }

    r.oldSwapchain_ = r.swapchain_;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = r.surface_;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = chosenFormat.format;
    createInfo.imageColorSpace = chosenFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = chosenPresentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = r.oldSwapchain_;

    uint32_t queueFamilyIndices[] = { r.graphicsFamily_, r.presentFamily_ };
    if (r.graphicsFamily_ != r.presentFamily_) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkResult result = pfn_vkCreateSwapchainKHR(r.device_, &createInfo, nullptr, &r.swapchain_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create swapchain\n");
        if (r.oldSwapchain_) {
            pfn_vkDestroySwapchainKHR(r.device_, r.oldSwapchain_, nullptr);
            r.oldSwapchain_ = VK_NULL_HANDLE;
        }
        return false;
    }

    if (r.oldSwapchain_) {
        pfn_vkDestroySwapchainKHR(r.device_, r.oldSwapchain_, nullptr);
        r.oldSwapchain_ = VK_NULL_HANDLE;
    }

    uint32_t swapImageCount = 0;
    pfn_vkGetSwapchainImagesKHR(r.device_, r.swapchain_, &swapImageCount, nullptr);
    r.swapchainImages_.resize(swapImageCount);
    pfn_vkGetSwapchainImagesKHR(r.device_, r.swapchain_, &swapImageCount, r.swapchainImages_.data());

    r.swapchainImageViews_.resize(swapImageCount);
    for (uint32_t i = 0; i < swapImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = r.swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chosenFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = pfn_vkCreateImageView(r.device_, &viewInfo, nullptr, &r.swapchainImageViews_[i]);
        if (result != VK_SUCCESS) {
            OutputDebugStringA("[VK] Failed to create swapchain image view\n");
            for (uint32_t created = 0; created < i; ++created) {
                if (r.swapchainImageViews_[created]) {
                    pfn_vkDestroyImageView(r.device_, r.swapchainImageViews_[created], nullptr);
                }
            }
            r.swapchainImageViews_.clear();
            r.swapchainImages_.clear();
            pfn_vkDestroySwapchainKHR(r.device_, r.swapchain_, nullptr);
            r.swapchain_ = VK_NULL_HANDLE;
            return false;
        }
    }

    r.swapchainFormat_ = chosenFormat.format;
    r.swapchainExtent_ = extent;

    return true;
}

void vk_swap::destroySwapchain(VkCtx& ctx) {
    auto& r = ctx.r;
    for (auto view : r.swapchainImageViews_) {
        if (view) pfn_vkDestroyImageView(r.device_, view, nullptr);
    }
    r.swapchainImageViews_.clear();
    r.swapchainImages_.clear();

    if (r.swapchain_) {
        pfn_vkDestroySwapchainKHR(r.device_, r.swapchain_, nullptr);
        r.swapchain_ = VK_NULL_HANDLE;
    }
}

void vk_swap::resize(VkCtx& ctx, uint32_t width, uint32_t height) {
    auto& r = ctx.r;
    if (!r.initialized_ || width == 0 || height == 0) return;
    pfn_vkDeviceWaitIdle(r.device_);
    destroySwapchain(ctx);
    createSwapchain(ctx, width, height);
}
