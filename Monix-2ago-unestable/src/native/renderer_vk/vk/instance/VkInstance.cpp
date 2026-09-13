#include "vk/instance/VkInstance.hpp"
#include "vulkan_renderer.h"
#include "vk/vk_globals.hpp"
#include <cstring>
#include <vector>

struct VkCtx { VulkanRenderer& r; };

bool vk_inst::createInstance(VkCtx& ctx) {
    auto& r = ctx.r;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Monix";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Monix";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    const char* requestedExtensions[4] = {};
    uint32_t extensionCount = 2;
    requestedExtensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;
    requestedExtensions[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    const char* requestedLayers[1] = {};
    uint32_t layerCount = 0;

    r.validationAvailable_ = false;
    r.validationEnabled_ = false;

    if (r.requestValidation_) {
        auto pfn_EnumLayers = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
            GetProcAddress(g_vkModule, "vkEnumerateInstanceLayerProperties"));
        if (pfn_EnumLayers) {
            uint32_t availLayerCount = 0;
            pfn_EnumLayers(&availLayerCount, nullptr);
            std::vector<VkLayerProperties> availLayers(availLayerCount);
            pfn_EnumLayers(&availLayerCount, availLayers.data());

            for (auto& layer : availLayers) {
                if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                    r.validationAvailable_ = true;
                    break;
                }
            }

            if (r.validationAvailable_) {
                requestedLayers[0] = "VK_LAYER_KHRONOS_validation";
                layerCount = 1;
                requestedExtensions[extensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
                r.validationEnabled_ = true;
                OutputDebugStringA("[VK] Validation layers ENABLED\n");
            } else {
                OutputDebugStringA("[VK] Validation layers NOT AVAILABLE — proceeding without\n");
            }
        }
    }

    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = requestedExtensions;
    createInfo.enabledLayerCount = layerCount;
    createInfo.ppEnabledLayerNames = requestedLayers;

    VkResult result = pfn_vkCreateInstance(&createInfo, nullptr, &r.instance_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create VkInstance\n");
        return false;
    }

    return true;
}

bool vk_inst::pickPhysicalDevice(VkCtx& ctx) {
    auto& r = ctx.r;

    uint32_t deviceCount = 0;
    pfn_vkEnumeratePhysicalDevices(r.instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        OutputDebugStringA("[VK] No Vulkan physical devices found\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    pfn_vkEnumeratePhysicalDevices(r.instance_, &deviceCount, devices.data());

    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        pfn_vkGetPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            r.physDev_ = dev;
            break;
        }
    }
    if (r.physDev_ == VK_NULL_HANDLE) {
        r.physDev_ = devices[0];
    }

    VkPhysicalDeviceProperties props{};
    pfn_vkGetPhysicalDeviceProperties(r.physDev_, &props);
    char buf[256];
    sprintf_s(buf, "[VK] Using GPU: %s\n", props.deviceName);
    OutputDebugStringA(buf);

    uint32_t queueFamilyCount = 0;
    pfn_vkGetPhysicalDeviceQueueFamilyProperties(r.physDev_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    pfn_vkGetPhysicalDeviceQueueFamilyProperties(r.physDev_, &queueFamilyCount, queueFamilies.data());

    r.graphicsFamily_ = UINT32_MAX;
    r.presentFamily_ = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            r.graphicsFamily_ = i;
        }
        VkBool32 presentSupport = false;
        pfn_vkGetPhysicalDeviceSurfaceSupportKHR(r.physDev_, i, r.surface_, &presentSupport);
        if (presentSupport) {
            r.presentFamily_ = i;
        }
        if (r.graphicsFamily_ != UINT32_MAX && r.presentFamily_ != UINT32_MAX) break;
    }

    if (r.graphicsFamily_ == UINT32_MAX || r.presentFamily_ == UINT32_MAX) {
        OutputDebugStringA("[VK] Required queue families not found\n");
        return false;
    }

    return true;
}

void vk_inst::loadInstanceFuncs(VkCtx& ctx) {
    auto& r = ctx.r;
    if (!r.instance_) return;
    pfn_vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkDestroyInstance"));
    pfn_vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkCreateDebugUtilsMessengerEXT"));
    pfn_vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkDestroyDebugUtilsMessengerEXT"));
    pfn_vkCreateWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkCreateWin32SurfaceKHR"));
    pfn_vkDestroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkDestroySurfaceKHR"));
    pfn_vkGetPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkGetPhysicalDeviceProperties"));
    pfn_vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkGetPhysicalDeviceMemoryProperties"));
    pfn_vkGetPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkGetPhysicalDeviceQueueFamilyProperties"));
    pfn_vkGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    pfn_vkGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    pfn_vkGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    pfn_vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkEnumeratePhysicalDevices"));
    pfn_vkEnumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        pfn_vkGetInstanceProcAddr(r.instance_, "vkEnumerateDeviceExtensionProperties"));
}

std::string vk_inst::getVendor(VkCtx& ctx) {
    auto& r = ctx.r;
    VkPhysicalDeviceProperties props{};
    pfn_vkGetPhysicalDeviceProperties(r.physDev_, &props);
    switch (props.vendorID) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x1414: return "Microsoft";
        default: return "Unknown";
    }
}

std::string vk_inst::getRenderer(VkCtx& ctx) {
    auto& r = ctx.r;
    VkPhysicalDeviceProperties props{};
    pfn_vkGetPhysicalDeviceProperties(r.physDev_, &props);
    return props.deviceName;
}

std::string vk_inst::getVersion(VkCtx& ctx) {
    auto& r = ctx.r;
    VkPhysicalDeviceProperties props{};
    pfn_vkGetPhysicalDeviceProperties(r.physDev_, &props);
    char buf[128];
    sprintf_s(buf, "Vulkan %d.%d.%d",
        VK_VERSION_MAJOR(props.apiVersion),
        VK_VERSION_MINOR(props.apiVersion),
        VK_VERSION_PATCH(props.apiVersion));
    return buf;
}
