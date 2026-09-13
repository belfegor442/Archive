#pragma once

#include <cstdint>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"

struct VkCtx;

namespace vk_val {
VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                        VkDebugUtilsMessageTypeFlagsEXT type,
                        const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                        void* userData);
}
