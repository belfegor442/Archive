#include "vk/validation/VkValidation.hpp"
#include "vulkan_renderer.h"

VkBool32 VKAPI_CALL vk_val::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData) {
    if (userData) {
        auto* renderer = reinterpret_cast<VulkanRenderer*>(userData);
        ValidationMessage msg;
        msg.severity = static_cast<uint32_t>(severity);
        msg.type = static_cast<uint32_t>(type);
        msg.message = callbackData->pMessage;
        renderer->addValidationMessage(std::move(msg));
    }
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        OutputDebugStringA(callbackData->pMessage);
        OutputDebugStringA("\n");
    }
    return VK_FALSE;
}
