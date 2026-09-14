// ============================================================================
// vulkan_renderer.cpp — Vulkan 1.3 renderer with Dynamic Rendering
// ============================================================================

#include "vulkan_renderer.h"

#include <cassert>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>

// ============================================================================
// Vulkan function pointers loaded dynamically from vulkan-1.dll
// ============================================================================


// ============================================================================
// Debug callback
// ============================================================================
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
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

// ============================================================================
// PipelineKey
// ============================================================================
bool PipelineKey::operator==(const PipelineKey& o) const {
    return vertModuleHash == o.vertModuleHash &&
           fragModuleHash == o.fragModuleHash &&
           colorFormat == o.colorFormat &&
           depthFormat == o.depthFormat &&
           hasDepth == o.hasDepth &&
           hasBlend == o.hasBlend;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================
VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    shutdown();
}

// ============================================================================
// enableValidation — call BEFORE initialize()
// ============================================================================
bool VulkanRenderer::enableValidation() {
    if (initialized_) return false;
    requestValidation_ = true;
    return true;
}

// ============================================================================
// addValidationMessage
// ============================================================================
void VulkanRenderer::addValidationMessage(ValidationMessage msg) {
    std::lock_guard<std::mutex> lock(validationMutex_);
    validationMessages_.push_back(std::move(msg));
}

// ============================================================================
// takeValidationMessages
// ============================================================================
std::vector<ValidationMessage> VulkanRenderer::takeValidationMessages() {
    std::lock_guard<std::mutex> lock(validationMutex_);
    return std::move(validationMessages_);
}

// ============================================================================
// clearValidationMessages
// ============================================================================
void VulkanRenderer::clearValidationMessages() {
    std::lock_guard<std::mutex> lock(validationMutex_);
    validationMessages_.clear();
}

// ============================================================================
// validationErrorCount / validationWarningCount / validationCriticalCount
// ============================================================================
uint32_t VulkanRenderer::validationErrorCount() const {
    std::lock_guard<std::mutex> lock(validationMutex_);
    uint32_t count = 0;
    for (auto& m : validationMessages_) {
        if (m.severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) count++;
    }
    return count;
}

uint32_t VulkanRenderer::validationWarningCount() const {
    std::lock_guard<std::mutex> lock(validationMutex_);
    uint32_t count = 0;
    for (auto& m : validationMessages_) {
        if (m.severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) count++;
    }
    return count;
}

uint32_t VulkanRenderer::validationCriticalCount() const {
    std::lock_guard<std::mutex> lock(validationMutex_);
    uint32_t count = 0;
    for (auto& m : validationMessages_) {
        if (m.severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) count++;
    }
    return count;
}

// ============================================================================
// initialize
// ============================================================================
bool VulkanRenderer::initialize(HWND hwnd, uint32_t width, uint32_t height) {
    if (initialized_) return true;

    hwnd_ = hwnd;

    // Load Vulkan DLL
    vk.vkModule = LoadLibraryA("vulkan-1.dll");
    if (!vk.vkModule) {
        OutputDebugStringA("[VK] Failed to load vulkan-1.dll\n");
        return false;
    }
    vulkanDll_ = vk.vkModule;

    vk.getInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(vk.vkModule, "vkGetInstanceProcAddr"));
    vk.getDeviceProcAddr =
        reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(vk.vkModule, "vkGetDeviceProcAddr"));
    if (!vk.getInstanceProcAddr) {
        OutputDebugStringA("[VK] vkGetInstanceProcAddr not found\n");
        shutdown();
        return false;
    }

    // Load global-level function pointers
    vk.createInstance = reinterpret_cast<PFN_vkCreateInstance>(
        GetProcAddress(vk.vkModule, "vkCreateInstance"));
    vk.destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        GetProcAddress(vk.vkModule, "vkDestroyInstance"));
    vk.createDevice = reinterpret_cast<PFN_vkCreateDevice>(
        GetProcAddress(vk.vkModule, "vkCreateDevice"));
    vk.enumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        GetProcAddress(vk.vkModule, "vkEnumeratePhysicalDevices"));

    // Create instance
    if (!createInstance()) { shutdown(); return false; }
    loadInstanceFuncs();

    // Create debug messenger (13.B)
    if (validationEnabled_ && vk.createDebugUtilsMessengerEXT) {
        VkDebugUtilsMessengerCreateInfoEXT dbgInfo{};
        dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgInfo.pfnUserCallback = debugCallback;
        dbgInfo.pUserData = this;

        VkResult dbgResult = vk.createDebugUtilsMessengerEXT(instance_, &dbgInfo, nullptr, &debugMessenger_);
        if (dbgResult == VK_SUCCESS) {
            OutputDebugStringA("[VK] Debug messenger created\n");
        } else {
            OutputDebugStringA("[VK] Failed to create debug messenger\n");
            debugMessenger_ = VK_NULL_HANDLE;
        }
    }

    // Create surface
    if (!createSurface(hwnd)) { shutdown(); return false; }

    // Pick physical device
    if (!pickPhysicalDevice()) { shutdown(); return false; }

    // Create logical device
    if (!createLogicalDevice()) { shutdown(); return false; }
    loadDeviceFuncs();
    vk.getDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    vk.getDeviceQueue(device_, presentFamily_, 0, &presentQueue_);

    // Create swapchain
    if (!createSwapchain(width, height)) { shutdown(); return false; }

    // Create sync objects
    if (!createSyncObjects()) { shutdown(); return false; }

    // Create command pool + buffers
    if (!createCommandPool()) { shutdown(); return false; }
    if (!allocateCommandBuffers()) { shutdown(); return false; }

    // Create descriptor pool
    if (!createDescriptorPool()) { shutdown(); return false; }

    // Create full-screen quad
    if (!createQuadBuffer()) { shutdown(); return false; }

    initialized_ = true;
    OutputDebugStringA("[VK] VulkanRenderer initialized successfully\n");
    return true;
}

// ============================================================================
// shutdown
// ============================================================================
void VulkanRenderer::shutdown() {

    if (device_) {
        vk.deviceWaitIdle(device_);
    }

    // Destroy quad
    if (quadCreated_) {
        destroyBuffer(quadVbo_);
        quadCreated_ = false;
    }

    // Destroy screenshot staging
    if (screenshotStagingCreated_) {
        destroyBuffer(screenshotStaging_);
        screenshotStagingCreated_ = false;
    }

    // Destroy upload resources
    if (uploadCreated_) {
        destroyBuffer(uploadStaging_);
        destroyImage(uploadImage_);
        uploadCreated_ = false;
    }

    // Destroy descriptor pool
    if (descPool_) {
        vk.destroyDescriptorPool(device_, descPool_, nullptr);
        descPool_ = VK_NULL_HANDLE;
    }

    // Destroy command pool
    if (cmdPool_) {
        vk.destroyCommandPool(device_, cmdPool_, nullptr);
        cmdPool_ = VK_NULL_HANDLE;
    }

    // Destroy sync objects
    if (syncCreated_) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
            vk.destroySemaphore(device_, imageAvailableSem_[i], nullptr);
            vk.destroySemaphore(device_, renderFinishedSem_[i], nullptr);
            vk.destroyFence(device_, inFlightFences_[i], nullptr);
        }
        syncCreated_ = false;
    }

    // Destroy swapchain
    destroySwapchain();

    // Destroy debug messenger
    if (debugMessenger_) {
        vk.destroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        debugMessenger_ = VK_NULL_HANDLE;
    }

    // Destroy surface
    if (surface_) {
        vk.destroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    // Destroy device
    if (device_) {
        vk.destroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    // Destroy instance
    if (instance_) {
        vk.destroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    initialized_ = false;

    // Reset all function pointers to prevent stale references
    vk.getInstanceProcAddr = nullptr;
    vk.getDeviceProcAddr = nullptr;
    vk.createInstance = nullptr;
    vk.destroyInstance = nullptr;
    vk.enumeratePhysicalDevices = nullptr;
    vk.getPhysicalDeviceProperties = nullptr;
    vk.getPhysicalDeviceMemoryProperties = nullptr;
    vk.getPhysicalDeviceQueueFamilyProperties = nullptr;
    vk.getPhysicalDeviceSurfaceSupportKHR = nullptr;
    vk.getPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    vk.getPhysicalDeviceSurfaceFormatsKHR = nullptr;
    vk.getPhysicalDeviceSurfacePresentModesKHR = nullptr;
    vk.createWin32SurfaceKHR = nullptr;
    vk.destroySurfaceKHR = nullptr;
    vk.createDebugUtilsMessengerEXT = nullptr;
    vk.destroyDebugUtilsMessengerEXT = nullptr;
    vk.enumerateDeviceExtensionProperties = nullptr;
    vk.createDevice = nullptr;
    vk.destroyDevice = nullptr;
    vk.getDeviceQueue = nullptr;
    vk.createSwapchainKHR = nullptr;
    vk.destroySwapchainKHR = nullptr;
    vk.getSwapchainImagesKHR = nullptr;
    vk.acquireNextImageKHR = nullptr;
    vk.queuePresentKHR = nullptr;
    vk.queueWaitIdle = nullptr;
    vk.deviceWaitIdle = nullptr;
    vk.createCommandPool = nullptr;
    vk.destroyCommandPool = nullptr;
    vk.allocateCommandBuffers = nullptr;
    vk.freeCommandBuffers = nullptr;
    vk.beginCommandBuffer = nullptr;
    vk.endCommandBuffer = nullptr;
    vk.cmdBeginRendering = nullptr;
    vk.cmdEndRendering = nullptr;
    vk.cmdBindPipeline = nullptr;
    vk.cmdSetViewport = nullptr;
    vk.cmdSetScissor = nullptr;
    vk.cmdDraw = nullptr;
    vk.cmdBlitImage = nullptr;
    vk.cmdCopyImageToBuffer = nullptr;
    vk.cmdPipelineBarrier = nullptr;
    vk.cmdBindVertexBuffers = nullptr;
    vk.cmdPushConstants = nullptr;
    vk.cmdBindDescriptorSets = nullptr;
    vk.createFence = nullptr;
    vk.destroyFence = nullptr;
    vk.waitForFences = nullptr;
    vk.resetFences = nullptr;
    vk.createSemaphore = nullptr;
    vk.destroySemaphore = nullptr;
    vk.createImage = nullptr;
    vk.destroyImage = nullptr;
    vk.getImageMemoryRequirements = nullptr;
    vk.allocateMemory = nullptr;
    vk.freeMemory = nullptr;
    vk.bindImageMemory = nullptr;
    vk.createImageView = nullptr;
    vk.destroyImageView = nullptr;
    vk.createSampler = nullptr;
    vk.destroySampler = nullptr;
    vk.createBuffer = nullptr;
    vk.destroyBuffer = nullptr;
    vk.getBufferMemoryRequirements = nullptr;
    vk.bindBufferMemory = nullptr;
    vk.mapMemory = nullptr;
    vk.unmapMemory = nullptr;
    vk.createShaderModule = nullptr;
    vk.destroyShaderModule = nullptr;
    vk.createPipelineLayout = nullptr;
    vk.destroyPipelineLayout = nullptr;
    vk.createGraphicsPipelines = nullptr;
    vk.destroyPipeline = nullptr;
    vk.createDescriptorSetLayout = nullptr;
    vk.destroyDescriptorSetLayout = nullptr;
    vk.createDescriptorPool = nullptr;
    vk.destroyDescriptorPool = nullptr;
    vk.allocateDescriptorSets = nullptr;
    vk.updateDescriptorSets = nullptr;
    vk.queueSubmit = nullptr;
    vk.cmdCopyBufferToImage = nullptr;
    vk.getImageSubresourceLayout = nullptr;
    vk.flushMappedMemoryRanges = nullptr;

    // Free Vulkan DLL
    if (vk.vkModule) {
        FreeLibrary(vk.vkModule);
        vk.vkModule = nullptr;
        vulkanDll_ = nullptr;
    }

    OutputDebugStringA("[VK] VulkanRenderer shut down\n");
}

// ============================================================================
// createInstance
// ============================================================================
bool VulkanRenderer::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Monix";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Monix";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // ---- Validation layer detection (13.A) ----
    const char* requestedExtensions[4] = {};
    uint32_t extensionCount = 2;
    requestedExtensions[0] = VK_KHR_SURFACE_EXTENSION_NAME;
    requestedExtensions[1] = VK_KHR_WIN32_SURFACE_EXTENSION_NAME;

    const char* requestedLayers[1] = {};
    uint32_t layerCount = 0;

    validationAvailable_ = false;
    validationEnabled_ = false;

    if (requestValidation_) {
        auto pfn_EnumLayers = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
            GetProcAddress(vk.vkModule, "vkEnumerateInstanceLayerProperties"));
        if (pfn_EnumLayers) {
            uint32_t availLayerCount = 0;
            pfn_EnumLayers(&availLayerCount, nullptr);
            std::vector<VkLayerProperties> availLayers(availLayerCount);
            pfn_EnumLayers(&availLayerCount, availLayers.data());

            for (auto& layer : availLayers) {
                if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
                    validationAvailable_ = true;
                    break;
                }
            }

            if (validationAvailable_) {
                requestedLayers[0] = "VK_LAYER_KHRONOS_validation";
                layerCount = 1;
                requestedExtensions[extensionCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
                validationEnabled_ = true;
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

    VkResult result = vk.createInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create VkInstance\n");
        return false;
    }

    return true;
}

// ============================================================================
// createSurface
// ============================================================================
bool VulkanRenderer::createSurface(HWND hwnd) {
    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hinstance = GetModuleHandleA(nullptr);
    createInfo.hwnd = hwnd;

    VkResult result = vk.createWin32SurfaceKHR(instance_, &createInfo, nullptr, &surface_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create Win32 surface\n");
        return false;
    }
    return true;
}
// ============================================================================
// pickPhysicalDevice
// ============================================================================
bool VulkanRenderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vk.enumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        OutputDebugStringA("[VK] No Vulkan physical devices found\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vk.enumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    // Prefer discrete GPU
    for (auto& dev : devices) {
        VkPhysicalDeviceProperties props{};
        vk.getPhysicalDeviceProperties(dev, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            physDev_ = dev;
            break;
        }
    }
    // Fall back to first device
    if (physDev_ == VK_NULL_HANDLE) {
        physDev_ = devices[0];
    }

    VkPhysicalDeviceProperties props{};
    vk.getPhysicalDeviceProperties(physDev_, &props);
    char buf[256];
    sprintf_s(buf, "[VK] Using GPU: %s\n", props.deviceName);
    OutputDebugStringA(buf);

    // Find queue families
    uint32_t queueFamilyCount = 0;
    vk.getPhysicalDeviceQueueFamilyProperties(physDev_, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vk.getPhysicalDeviceQueueFamilyProperties(physDev_, &queueFamilyCount, queueFamilies.data());

    graphicsFamily_ = UINT32_MAX;
    presentFamily_ = UINT32_MAX;

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily_ = i;
        }
        VkBool32 presentSupport = false;
        vk.getPhysicalDeviceSurfaceSupportKHR(physDev_, i, surface_, &presentSupport);
        if (presentSupport) {
            presentFamily_ = i;
        }
        if (graphicsFamily_ != UINT32_MAX && presentFamily_ != UINT32_MAX) break;
    }

    if (graphicsFamily_ == UINT32_MAX || presentFamily_ == UINT32_MAX) {
        OutputDebugStringA("[VK] Required queue families not found\n");
        return false;
    }

    return true;
}

// ============================================================================
// createLogicalDevice
// ============================================================================
bool VulkanRenderer::createLogicalDevice() {
    float queuePriority = 1.0f;

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    std::vector<uint32_t> uniqueFamilies;
    if (graphicsFamily_ == presentFamily_) {
        uniqueFamilies.push_back(graphicsFamily_);
    } else {
        uniqueFamilies.push_back(graphicsFamily_);
        uniqueFamilies.push_back(presentFamily_);
    }

    for (auto family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qi{};
        qi.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qi.queueFamilyIndex = family;
        qi.queueCount = 1;
        qi.pQueuePriorities = &queuePriority;
        queueInfos.push_back(qi);
    }

    memset(&vk13Features_, 0, sizeof(vk13Features_));
    vk13Features_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vk13Features_.dynamicRendering = VK_TRUE;
    vk13Features_.synchronization2 = VK_TRUE;

    memset(&dynamicRenderingFeat_, 0, sizeof(dynamicRenderingFeat_));
    dynamicRenderingFeat_.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeat_.pNext = &vk13Features_;
    dynamicRenderingFeat_.dynamicRendering = VK_TRUE;

    const char* extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &dynamicRenderingFeat_;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size());
    createInfo.pQueueCreateInfos = queueInfos.data();
    createInfo.enabledExtensionCount = 1;
    createInfo.ppEnabledExtensionNames = extensions;

    VkResult result = vk.createDevice(physDev_, &createInfo, nullptr, &device_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create VkDevice\n");
        return false;
    }

    return true;
}

// ============================================================================
// createSwapchain
// ============================================================================
bool VulkanRenderer::createSwapchain(uint32_t width, uint32_t height) {
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR caps{};
    vk.getPhysicalDeviceSurfaceCapabilitiesKHR(physDev_, surface_, &caps);

    uint32_t formatCount = 0;
    vk.getPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vk.getPhysicalDeviceSurfaceFormatsKHR(physDev_, surface_, &formatCount, formats.data());

    uint32_t presentModeCount = 0;
    vk.getPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vk.getPhysicalDeviceSurfacePresentModesKHR(physDev_, surface_, &presentModeCount, presentModes.data());

    // Choose surface format (prefer SRGB B8G8R8A8)
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
    // Prefer UNORM for CRT accuracy
    for (auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM) {
            chosenFormat = f;
            break;
        }
    }

    // Choose present mode (prefer FIFO for vsync, mailbox for low latency)
    VkPresentModeKHR chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (auto m : presentModes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            chosenPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }

    // Choose extent
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

    // Store old swapchain for recreation
    oldSwapchain_ = swapchain_;

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface_;
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
    createInfo.oldSwapchain = oldSwapchain_;

    uint32_t queueFamilyIndices[] = { graphicsFamily_, presentFamily_ };
    if (graphicsFamily_ != presentFamily_) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VkResult result = vk.createSwapchainKHR(device_, &createInfo, nullptr, &swapchain_);
    if (result != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create swapchain\n");
        return false;
    }

    // Destroy old swapchain
    if (oldSwapchain_) {
        vk.destroySwapchainKHR(device_, oldSwapchain_, nullptr);
        oldSwapchain_ = VK_NULL_HANDLE;
    }

    // Get swapchain images
    uint32_t swapImageCount = 0;
    vk.getSwapchainImagesKHR(device_, swapchain_, &swapImageCount, nullptr);
    swapchainImages_.resize(swapImageCount);
    vk.getSwapchainImagesKHR(device_, swapchain_, &swapImageCount, swapchainImages_.data());

    // Create image views
    swapchainImageViews_.resize(swapImageCount);
    for (uint32_t i = 0; i < swapImageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = chosenFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        result = vk.createImageView(device_, &viewInfo, nullptr, &swapchainImageViews_[i]);
        if (result != VK_SUCCESS) {
            OutputDebugStringA("[VK] Failed to create swapchain image view\n");
            return false;
        }
    }

    swapchainFormat_ = chosenFormat.format;
    swapchainExtent_ = extent;

    return true;
}

// ============================================================================
// destroySwapchain
// ============================================================================
void VulkanRenderer::destroySwapchain() {
    for (auto view : swapchainImageViews_) {
        if (view) vk.destroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_) {
        vk.destroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

// ============================================================================
// resize
// ============================================================================
void VulkanRenderer::resize(uint32_t width, uint32_t height) {
    if (!initialized_ || width == 0 || height == 0) return;
    vk.deviceWaitIdle(device_);
    destroySwapchain();
    if (!createSwapchain(width, height)) {
        OutputDebugStringA("[VK] resize: FAILED to recreate swapchain — renderer in degraded state\n");
    }
}

// ============================================================================
// createSyncObjects
// ============================================================================
bool VulkanRenderer::createSyncObjects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
        if (vk.createSemaphore(device_, &semInfo, nullptr, &imageAvailableSem_[i]) != VK_SUCCESS ||
            vk.createSemaphore(device_, &semInfo, nullptr, &renderFinishedSem_[i]) != VK_SUCCESS ||
            vk.createFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]) != VK_SUCCESS) {
            OutputDebugStringA("[VK] Failed to create sync objects\n");
            return false;
        }
    }

    syncCreated_ = true;
    return true;
}

// ============================================================================
// createCommandPool
// ============================================================================
bool VulkanRenderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;

    VkResult result = vk.createCommandPool(device_, &poolInfo, nullptr, &cmdPool_);
    return result == VK_SUCCESS;
}

// ============================================================================
// allocateCommandBuffers
// ============================================================================
bool VulkanRenderer::allocateCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = cmdPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = kMaxFramesInFlight;

    VkResult result = vk.allocateCommandBuffers(device_, &allocInfo, cmdBuffers_.data());
    return result == VK_SUCCESS;
}

// ============================================================================
// createDescriptorPool
// ============================================================================
bool VulkanRenderer::createDescriptorPool() {
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxDescriptorSets },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxUniformBuffers },
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = kMaxDescriptorSets;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes = poolSizes;

    VkResult result = vk.createDescriptorPool(device_, &poolInfo, nullptr, &descPool_);
    return result == VK_SUCCESS;
}

// ============================================================================
// createQuadBuffer
// ============================================================================
bool VulkanRenderer::createQuadBuffer() {
    // Full-screen quad: two triangles covering [-1,1] with tex coords [0,1]
    // Using triangle strip order: BL, BR, TL, TR
    // DIBSection with positive biHeight = bottom-up in Win32.
    // Buffer row 0 = bottom of visual → Vulkan image row 0 (texcoord y=0).
    // To display correctly: screen top needs y=1 (GDI top), screen bottom needs y=0 (GDI bottom).
    QuadVertex quadVerts[] = {
        {{ -1.0f, -1.0f }, { 0.0f, 1.0f }},  // bottom-left  → GDI top
        {{  1.0f, -1.0f }, { 1.0f, 1.0f }},  // bottom-right → GDI top
        {{ -1.0f,  1.0f }, { 0.0f, 0.0f }},  // top-left     → GDI bottom
        {{  1.0f,  1.0f }, { 1.0f, 0.0f }},  // top-right    → GDI bottom
    };

    VkDeviceSize size = sizeof(quadVerts);
    quadVbo_ = createBuffer(size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (quadVbo_.buffer == VK_NULL_HANDLE) return false;

    uploadBuffer(quadVbo_, quadVerts, size);
    quadCreated_ = true;

    char dbg[128];
    sprintf_s(dbg, "[VK] createQuadBuffer: BL tex=(%.1f,%.1f) TL tex=(%.1f,%.1f)\n",
        quadVerts[0].texCoord[0], quadVerts[0].texCoord[1],
        quadVerts[2].texCoord[0], quadVerts[2].texCoord[1]);
    OutputDebugStringA(dbg);

    return true;
}

// ============================================================================
// beginFrame
// ============================================================================
bool VulkanRenderer::beginFrame(uint32_t width, uint32_t height) {
    if (!initialized_ || frameActive_) return false;

    // Wait for previous frame (1 second timeout to avoid blocking message loop)
    VkResult fenceResult = vk.waitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, 1000000000ULL);
    if (fenceResult == VK_TIMEOUT) {
        OutputDebugStringA("[VK] beginFrame: fence wait timeout, skipping frame\n");
        return false;
    }
    VkResult resetResult = vk.resetFences(device_, 1, &inFlightFences_[currentFrame_]);
    if (resetResult != VK_SUCCESS) {
        OutputDebugStringA("[VK] vkResetFences failed\n");
    }

    // Acquire next swapchain image
    VkResult result = vk.acquireNextImageKHR(
        device_, swapchain_, UINT64_MAX,
        imageAvailableSem_[currentFrame_], VK_NULL_HANDLE,
        &currentImageIndex_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        resize(width, height);
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    // Begin command buffer
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult beginResult = vk.beginCommandBuffer(cmdBuffers_[currentFrame_], &beginInfo);
    if (beginResult != VK_SUCCESS) {
        OutputDebugStringA("[VK] vkBeginCommandBuffer failed\n");
        return false;
    }
    frameActive_ = true;
    return true;
}

// ============================================================================
// endFrame
// ============================================================================
void VulkanRenderer::endFrame() {
    if (!frameActive_) return;

    // If screenshot was requested but not yet copied (e.g. no preset rendering),
    // record the copy now from the current swapchain image.
    if (screenshot_.pending && screenshot_.width > 0 && screenshot_.height > 0 && !screenshotCopied_) {
        ensureScreenshotStaging(screenshot_.width, screenshot_.height);
        // The swapchain is in PRESENT_SRC from renderPreset's last pass transition
        // Transition to TRANSFER_SRC, copy, then back to PRESENT_SRC
        VkImage swapImg = currentSwapchainImage();
        if (swapImg) {
            cmdTransitionImage(swapImg,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkImageResource swapchainRes{};
            swapchainRes.image = swapImg;
            cmdCopyImageToBuffer(swapchainRes, screenshotStaging_, screenshot_.width, screenshot_.height);
            cmdTransitionImage(swapImg,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            screenshotCopied_ = true;
        }
    }

    VkResult endResult = vk.endCommandBuffer(cmdBuffers_[currentFrame_]);
    if (endResult != VK_SUCCESS) {
        OutputDebugStringA("[VK] vkEndCommandBuffer failed\n");
    }
    frameActive_ = false;
}

// ============================================================================
// present
// ============================================================================
void VulkanRenderer::present() {
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSem_[currentFrame_];
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffers_[currentFrame_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSem_[currentFrame_];

    VkResult submitResult = vk.queueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]);
    if (submitResult != VK_SUCCESS) {
        OutputDebugStringA("[VK] vkQueueSubmit failed\n");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSem_[currentFrame_];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImageIndex_;

    VkResult presentResult = vk.queuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult != VK_SUCCESS) {
        OutputDebugStringA("[VK] vkQueuePresentKHR failed\n");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

// ============================================================================
// waitForIdle
// ============================================================================
void VulkanRenderer::waitForIdle() {
    if (device_) {
        vk.deviceWaitIdle(device_);
    }
}

// ============================================================================
// Memory helpers
// ============================================================================
uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vk.getPhysicalDeviceMemoryProperties(physDev_, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    OutputDebugStringA("[VK] Failed to find suitable memory type\n");
    return UINT32_MAX;
}

bool VulkanRenderer::allocateImageMemory(VkImage image, VkMemoryPropertyFlags props, VkDeviceMemory* outMem) {
    VkMemoryRequirements memReqs{};
    vk.getImageMemoryRequirements(device_, image, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, props);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) return false;

    VkResult result = vk.allocateMemory(device_, &allocInfo, nullptr, outMem);
    if (result != VK_SUCCESS) return false;

    if (vk.bindImageMemory(device_, image, *outMem, 0) != VK_SUCCESS) {
        vk.freeMemory(device_, *outMem, nullptr);
        *outMem = VK_NULL_HANDLE;
        return false;
    }
    return true;
}

// ============================================================================
// transitionImageLayoutImmediate — one-shot command buffer for layout transitions
// ============================================================================
bool VulkanRenderer::transitionImageLayoutImmediate(VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = cmdPool_;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vk.allocateCommandBuffers(device_, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vk.beginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vk.cmdPipelineBarrier(cmd, srcStage, dstStage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    vk.endCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vk.queueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
    vk.queueWaitIdle(graphicsQueue_);

    vk.freeCommandBuffers(device_, cmdPool_, 1, &cmd);
    return true;
}

// ============================================================================
// createImage
// ============================================================================
VkImageResource VulkanRenderer::createImage(uint32_t w, uint32_t h, VkFormat fmt,
                                             bool renderTarget, bool cpuReadable) {
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

    if (vk.createImage(device_, &imageInfo, nullptr, &result.image) != VK_SUCCESS) {
        return result;
    }

    // Allocate memory
    VkMemoryPropertyFlags memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (cpuReadable) {
        memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    if (!allocateImageMemory(result.image, memProps, &result.memory)) {
        vk.destroyImage(device_, result.image, nullptr);
        result.image = VK_NULL_HANDLE;
        return result;
    }

    // Create image view
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

    if (vk.createImageView(device_, &viewInfo, nullptr, &result.view) != VK_SUCCESS) {
        vk.destroyImage(device_, result.image, nullptr);
        vk.freeMemory(device_, result.memory, nullptr);
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

// ============================================================================
// destroyImage
// ============================================================================
void VulkanRenderer::destroyImage(VkImageResource& img) {
    if (img.view) vk.destroyImageView(device_, img.view, nullptr);
    if (img.image) vk.destroyImage(device_, img.image, nullptr);
    if (img.memory) vk.freeMemory(device_, img.memory, nullptr);
    img = {};
}

// ============================================================================
// createSampler
// ============================================================================
VkSamplerResource VulkanRenderer::createSampler(VkFilter minFilter, VkFilter magFilter,
                                                 VkSamplerAddressMode wrapS,
                                                 VkSamplerAddressMode wrapT,
                                                 float borderColorR, float borderColorG,
                                                 float borderColorB, float borderColorA) {
    VkSamplerResource result{};

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
    samplerInfo.addressModeU = wrapS;
    samplerInfo.addressModeV = wrapT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;

    // Map wrap modes
    if (wrapS == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER ||
        wrapT == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER) {
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        // Could use custom border color with VK_EXT_custom_border_color
    }

    if (vk.createSampler(device_, &samplerInfo, nullptr, &result.sampler) != VK_SUCCESS) {
        OutputDebugStringA("[VK] Failed to create sampler\n");
    }
    return result;
}

// ============================================================================
// destroySampler
// ============================================================================
void VulkanRenderer::destroySampler(VkSamplerResource& s) {
    if (s.sampler) vk.destroySampler(device_, s.sampler, nullptr);
    s = {};
}

// ============================================================================
// createBuffer
// ============================================================================
VkBufferResource VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                               VkMemoryPropertyFlags props) {
    VkBufferResource result{};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vk.createBuffer(device_, &bufferInfo, nullptr, &result.buffer) != VK_SUCCESS) {
        return result;
    }

    VkMemoryRequirements memReqs{};
    vk.getBufferMemoryRequirements(device_, result.buffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(memReqs.memoryTypeBits, props);

    if (allocInfo.memoryTypeIndex == UINT32_MAX) {
        vk.destroyBuffer(device_, result.buffer, nullptr);
        result.buffer = VK_NULL_HANDLE;
        return result;
    }

    if (vk.allocateMemory(device_, &allocInfo, nullptr, &result.memory) != VK_SUCCESS) {
        vk.destroyBuffer(device_, result.buffer, nullptr);
        result.buffer = VK_NULL_HANDLE;
        return result;
    }

    if (vk.bindBufferMemory(device_, result.buffer, result.memory, 0) != VK_SUCCESS) {
        OutputDebugStringA("[VK] vkBindBufferMemory failed\n");
        vk.destroyBuffer(device_, result.buffer, nullptr);
        vk.freeMemory(device_, result.memory, nullptr);
        return {};
    }
    result.size = size;

    // Map if host visible
    if (props & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        if (vk.mapMemory(device_, result.memory, 0, size, 0, &result.mapped) != VK_SUCCESS) {
            OutputDebugStringA("[VK] createBuffer: vkMapMemory failed\n");
            result.mapped = nullptr;
        }
    }

    return result;
}

// ============================================================================
// destroyBuffer
// ============================================================================
void VulkanRenderer::destroyBuffer(VkBufferResource& buf) {
    if (buf.mapped) {
        vk.unmapMemory(device_, buf.memory);
        buf.mapped = nullptr;
    }
    if (buf.buffer) vk.destroyBuffer(device_, buf.buffer, nullptr);
    if (buf.memory) vk.freeMemory(device_, buf.memory, nullptr);
    buf = {};
}

// ============================================================================
// uploadBuffer
// ============================================================================
void VulkanRenderer::uploadBuffer(VkBufferResource& dst, const void* data, VkDeviceSize size) {
    if (!dst.mapped || !data || size == 0 || size > dst.size) return;
    memcpy(dst.mapped, data, static_cast<size_t>(size));
}

// ============================================================================
// createDescriptorSetLayout
// ============================================================================
VkDescriptorSetLayout VulkanRenderer::createDescriptorSetLayout(
    std::span<const VkDescriptorSetLayoutBinding> bindings) {
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    vk.createDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout);
    return layout;
}

// ============================================================================
// destroyDescriptorSetLayout
// ============================================================================
void VulkanRenderer::destroyDescriptorSetLayout(VkDescriptorSetLayout layout) {
    if (layout) vk.destroyDescriptorSetLayout(device_, layout, nullptr);
}

// ============================================================================
// allocateDescriptorSet
// ============================================================================
VkDescriptorSet VulkanRenderer::allocateDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    vk.allocateDescriptorSets(device_, &allocInfo, &set);
    return set;
}

// ============================================================================
// updateDescriptorSetTexture
// ============================================================================
void VulkanRenderer::updateDescriptorSetTexture(VkDescriptorSet set, uint32_t binding,
                                                 VkImageView view, VkSampler sampler,
                                                 VkImageLayout layout) {
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = layout;
    imageInfo.imageView = view;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vk.updateDescriptorSets(device_, 1, &write, 0, nullptr);
}

// ============================================================================
// updateDescriptorSetUniform
// ============================================================================
void VulkanRenderer::updateDescriptorSetUniform(VkDescriptorSet set, uint32_t binding,
                                                  VkBuffer buffer, VkDeviceSize range) {
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vk.updateDescriptorSets(device_, 1, &write, 0, nullptr);
}

// ============================================================================
// createPipelineLayout
// ============================================================================
VkPipelineLayout VulkanRenderer::createPipelineLayout(
    std::span<const VkDescriptorSetLayout> descLayouts,
    std::span<const VkPushConstantRange> pushRanges) {
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(descLayouts.size());
    layoutInfo.pSetLayouts = descLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushRanges.size());
    layoutInfo.pPushConstantRanges = pushRanges.data();

    VkPipelineLayout layout = VK_NULL_HANDLE;
    vk.createPipelineLayout(device_, &layoutInfo, nullptr, &layout);
    return layout;
}

// ============================================================================
// destroyPipelineLayout
// ============================================================================
void VulkanRenderer::destroyPipelineLayout(VkPipelineLayout layout) {
    if (layout) vk.destroyPipelineLayout(device_, layout, nullptr);
}

// ============================================================================
// createGraphicsPipeline
// ============================================================================
VkPipeline VulkanRenderer::createGraphicsPipeline(
    std::span<const uint32_t> vertSpirv,
    std::span<const uint32_t> fragSpirv,
    VkPipelineLayout layout,
    VkFormat colorFormat,
    VkFormat depthFormat,
    bool hasBlend,
    VkVertexInputBindingDescription vertBinding,
    std::span<const VkVertexInputAttributeDescription> vertAttrs) {

    // Create shader modules
    VkShaderModule vertMod = createShaderModule(vertSpirv);
    VkShaderModule fragMod = createShaderModule(fragSpirv);
    if (!vertMod || !fragMod) {
        destroyShaderModule(vertMod);
        destroyShaderModule(fragMod);
        return VK_NULL_HANDLE;
    }

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertMod;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragMod;
    fragStage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    // Vertex input
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription bindingDesc{};
    if (vertBinding.binding != 0 || vertBinding.stride != 0) {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &vertBinding;
    }
    if (!vertAttrs.empty()) {
        vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertAttrs.size());
        vertexInput.pVertexAttributeDescriptions = vertAttrs.data();
    }

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // Viewport/scissor (dynamic)
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = hasBlend ? VK_TRUE : VK_FALSE;
    if (hasBlend) {
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    // Dynamic state
    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    // Dynamic rendering — no VkRenderPass needed
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorFormat;
    renderingInfo.depthAttachmentFormat = depthFormat;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = vk.createGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    destroyShaderModule(vertMod);
    destroyShaderModule(fragMod);

    if (result != VK_SUCCESS) {
        FILE* pf = nullptr;
        fopen_s(&pf, "vk_pipeline_fail.log", "a");
        if (pf) {
            fprintf(pf, "VkCreateGraphicsPipelines FAILED: VkResult=%d\n", (int)result);
            fclose(pf);
        }
        return VK_NULL_HANDLE;
    }
    return pipeline;
}

// ============================================================================
// destroyPipeline
// ============================================================================
void VulkanRenderer::destroyPipeline(VkPipeline pipeline) {
    if (pipeline) vk.destroyPipeline(device_, pipeline, nullptr);
}

// ============================================================================
// createShaderModule
// ============================================================================
VkShaderModule VulkanRenderer::createShaderModule(std::span<const uint32_t> spirv) {
    if (spirv.empty()) {
        OutputDebugStringA("[VK] createShaderModule: EMPTY spirv\n");
        return VK_NULL_HANDLE;
    }
    if (spirv[0] != 0x07230203) {
        char buf[128];
        sprintf_s(buf, "[VK] createShaderModule: BAD magic 0x%08X (expected 0x07230203), size=%zu\n",
            spirv[0], spirv.size());
        OutputDebugStringA(buf);
        return VK_NULL_HANDLE;
    }
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(uint32_t);
    createInfo.pCode = spirv.data();

    // Heap-allocate shared state so detached thread never references stack locals
    auto sharedResult = std::make_shared<std::atomic<VkResult>>(VK_ERROR_UNKNOWN);
    auto sharedMod = std::make_shared<std::atomic<VkShaderModule>>(VK_NULL_HANDLE);
    auto sharedDone = std::make_shared<std::atomic<bool>>(false);

    // Run vkCreateShaderModule on a background thread with timeout
    // Known driver bug: some drivers hang in vkCreateShaderModule with large SPIR-V
    std::thread worker([this, createInfo, sharedResult, sharedMod, sharedDone]() {
        VkShaderModule mod = VK_NULL_HANDLE;
        VkResult r = vk.createShaderModule(device_, &createInfo, nullptr, &mod);
        sharedMod->store(mod, std::memory_order_release);
        sharedResult->store(r, std::memory_order_release);
        sharedDone->store(true, std::memory_order_release);
    });

    // Wait up to 5 seconds for shader module creation
    constexpr int kTimeoutMs = 5000;
    auto start = std::chrono::steady_clock::now();
    while (!sharedDone->load(std::memory_order_acquire)) {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() >= kTimeoutMs) {
            char buf[256];
            sprintf_s(buf, "[VK] createShaderModule: TIMEOUT after %d ms (driver hang on %zu words)\n",
                kTimeoutMs, spirv.size());
            OutputDebugStringA(buf);
            worker.detach();
            return VK_NULL_HANDLE;
        }
        Sleep(10);
    }
    worker.join();

    VkResult createResult = sharedResult->load(std::memory_order_acquire);
    VkShaderModule mod = sharedMod->load(std::memory_order_acquire);

    char buf[128];
    sprintf_s(buf, "[VK] createShaderModule: result=%d, mod=%p\n", createResult, (void*)mod);
    OutputDebugStringA(buf);
    return mod;
}

// ============================================================================
// destroyShaderModule
// ============================================================================
void VulkanRenderer::destroyShaderModule(VkShaderModule mod) {
    if (mod) vk.destroyShaderModule(device_, mod, nullptr);
}

// ============================================================================
// cmdBeginRendering — Dynamic Rendering (no VkRenderPass)
// ============================================================================
void VulkanRenderer::cmdBeginRendering(VkImageView colorTarget, uint32_t w, uint32_t h,
                                        VkClearValue clear) {
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorTarget;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue = clear;

    VkRenderingInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderInfo.renderArea = { {0, 0}, {w, h} };
    renderInfo.layerCount = 1;
    renderInfo.colorAttachmentCount = 1;
    renderInfo.pColorAttachments = &colorAttachment;

    vk.cmdBeginRendering(cmdBuffers_[currentFrame_], &renderInfo);
}

// ============================================================================
// cmdEndRendering
// ============================================================================
void VulkanRenderer::cmdEndRendering() {
    vk.cmdEndRendering(cmdBuffers_[currentFrame_]);
}

// ============================================================================
// cmdBindPipeline
// ============================================================================
void VulkanRenderer::cmdBindPipeline(VkPipeline pipeline, VkPipelineLayout layout) {
    vk.cmdBindPipeline(cmdBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

// ============================================================================
// cmdBindDescriptorSet
// ============================================================================
void VulkanRenderer::cmdBindDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set, uint32_t setIdx) {
    vk.cmdBindDescriptorSets(cmdBuffers_[currentFrame_], VK_PIPELINE_BIND_POINT_GRAPHICS,
        layout, setIdx, 1, &set, 0, nullptr);
}

// ============================================================================
// cmdBindVertexBuffer
// ============================================================================
void VulkanRenderer::cmdBindVertexBuffer(VkBufferResource& buf) {
    VkBuffer buffers[] = { buf.buffer };
    VkDeviceSize offsets[] = { 0 };
    vk.cmdBindVertexBuffers(cmdBuffers_[currentFrame_], 0, 1, buffers, offsets);
}

// ============================================================================
// cmdSetViewport
// ============================================================================
void VulkanRenderer::cmdSetViewport(uint32_t w, uint32_t h) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(w);
    viewport.height = static_cast<float>(h);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vk.cmdSetViewport(cmdBuffers_[currentFrame_], 0, 1, &viewport);
}

// ============================================================================
// cmdSetScissor
// ============================================================================
void VulkanRenderer::cmdSetScissor(uint32_t w, uint32_t h) {
    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = { w, h };
    vk.cmdSetScissor(cmdBuffers_[currentFrame_], 0, 1, &scissor);
}

// ============================================================================
// cmdPushConstants
// ============================================================================
void VulkanRenderer::cmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stage,
                                       uint32_t offset, uint32_t size, const void* data) {
    vk.cmdPushConstants(cmdBuffers_[currentFrame_], layout, stage, offset, size, data);
}

// ============================================================================
// cmdDraw
// ============================================================================
void VulkanRenderer::cmdDraw(uint32_t vertexCount, uint32_t firstVertex) {
    vk.cmdDraw(cmdBuffers_[currentFrame_], vertexCount, 1, firstVertex, 0);
}

// ============================================================================
// cmdBlitImage
// ============================================================================
void VulkanRenderer::cmdBlitImage(VkImageResource& src, VkImageResource& dst,
                                   uint32_t srcW, uint32_t srcH, uint32_t dstW, uint32_t dstH) {
    VkImageBlit region{};
    region.srcOffsets[0] = { 0, 0, 0 };
    region.srcOffsets[1] = { static_cast<int32_t>(srcW), static_cast<int32_t>(srcH), 1 };
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.layerCount = 1;
    region.dstOffsets[0] = { 0, 0, 0 };
    region.dstOffsets[1] = { static_cast<int32_t>(dstW), static_cast<int32_t>(dstH), 1 };
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.mipLevel = 0;
    region.dstSubresource.baseArrayLayer = 0;
    region.dstSubresource.layerCount = 1;

    vk.cmdBlitImage(cmdBuffers_[currentFrame_],
        src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, VK_FILTER_LINEAR);
}

// ============================================================================
// cmdCopyImageToBuffer
// ============================================================================
void VulkanRenderer::cmdCopyImageToBuffer(VkImageResource& src, VkBufferResource& dst,
                                           uint32_t w, uint32_t h) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { w, h, 1 };

    vk.cmdCopyImageToBuffer(cmdBuffers_[currentFrame_],
        src.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        dst.buffer, 1, &region);
}

// ============================================================================
// cmdCopyBufferToImage
// ============================================================================
void VulkanRenderer::cmdCopyBufferToImage(VkBufferResource& src, VkImageResource& dst,
                                           uint32_t w, uint32_t h) {
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = { w, h, 1 };

    vk.cmdCopyBufferToImage(cmdBuffers_[currentFrame_],
        src.buffer, dst.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region);
}

// ============================================================================
// uploadTexture — upload BGRA pixels to swapchain via staging buffer
// ============================================================================
bool VulkanRenderer::uploadTexture(const void* bgraPixels, uint32_t w, uint32_t h) {
    return uploadTextureToImage(bgraPixels, w, h, true);
}

bool VulkanRenderer::uploadTextureToImage(const void* bgraPixels, uint32_t w, uint32_t h, bool blitToSwapchain) {
    if (!frameActive_ || !bgraPixels || w == 0 || h == 0) return false;

    // Recreate staging buffer if size changed
    VkDeviceSize dataSize = static_cast<VkDeviceSize>(w) * h * 4;
    if (!uploadCreated_ || uploadStaging_.size < dataSize) {
        if (uploadCreated_) {
            destroyBuffer(uploadStaging_);
            destroyImage(uploadImage_);
        }
        uploadStaging_ = createBuffer(dataSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        uploadImage_ = createImage(w, h, VK_FORMAT_B8G8R8A8_UNORM, false, false);
        if (!uploadStaging_.buffer || !uploadImage_.image) {
            return false;
        }
        uploadCreated_ = true;
    } else if (uploadImage_.width != w || uploadImage_.height != h) {
        destroyImage(uploadImage_);
        uploadImage_ = createImage(w, h, VK_FORMAT_B8G8R8A8_UNORM, false, false);
        if (!uploadImage_.image) return false;
    }

    // Upload pixels to staging buffer
    uploadBuffer(uploadStaging_, bgraPixels, dataSize);

    // Transition upload image to TRANSFER_DST
    cmdTransitionLayout(uploadImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Copy buffer → image
    cmdCopyBufferToImage(uploadStaging_, uploadImage_, w, h);

    if (blitToSwapchain) {
        // Transition upload image to TRANSFER_SRC for blit
        cmdTransitionLayout(uploadImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        // Transition swapchain to TRANSFER_DST
        cmdTransitionImage(currentSwapchainImage(),
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        // Blit upload image → swapchain (flip Y: GDI DIBSection is bottom-up, Vulkan is top-down)
        VkImageBlit region{};
        region.srcOffsets[0] = { 0, static_cast<int32_t>(h), 0 };
        region.srcOffsets[1] = { static_cast<int32_t>(w), 0, 1 };
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.dstOffsets[0] = { 0, 0, 0 };
        region.dstOffsets[1] = { static_cast<int32_t>(swapchainExtent_.width),
                                  static_cast<int32_t>(swapchainExtent_.height), 1 };
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;

        vk.cmdBlitImage(cmdBuffers_[currentFrame_],
            uploadImage_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            currentSwapchainImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region, VK_FILTER_LINEAR);

        // Transition swapchain to present
        cmdTransitionImage(currentSwapchainImage(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    } else {
        // Leave uploadImage_ in SHADER_READ_ONLY for preset rendering
        // Must use cmdTransitionLayout to update currentLayout tracking
        cmdTransitionLayout(uploadImage_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    return true;
}

// ============================================================================
// cmdTransitionLayout
// ============================================================================
void VulkanRenderer::cmdTransitionLayout(VkImageResource& img, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = img.currentLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage, dstStage;

    if (img.currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (img.currentLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vk.cmdPipelineBarrier(cmdBuffers_[currentFrame_], srcStage, dstStage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    img.currentLayout = newLayout;
}

// ============================================================================
// cmdTransitionImage — raw VkImage version (for swapchain images)
// ============================================================================
void VulkanRenderer::cmdTransitionImage(VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout,
                                         VkImageSubresourceRange range) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = img;
    barrier.subresourceRange = range;

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }

    vk.cmdPipelineBarrier(cmdBuffers_[currentFrame_], srcStage, dstStage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);
}

// ============================================================================
// drawFullScreenQuad
// ============================================================================
void VulkanRenderer::drawFullScreenQuad() {
    VkBuffer buffers[] = { quadVbo_.buffer };
    VkDeviceSize offsets[] = { 0 };
    vk.cmdBindVertexBuffers(cmdBuffers_[currentFrame_], 0, 1, buffers, offsets);
    vk.cmdDraw(cmdBuffers_[currentFrame_], 4, 1, 0, 0);
}

// ============================================================================
// currentSwapchainView
// ============================================================================
VkImageView VulkanRenderer::currentSwapchainView() const {
    if (currentImageIndex_ < swapchainImageViews_.size()) {
        return swapchainImageViews_[currentImageIndex_];
    }
    return VK_NULL_HANDLE;
}

VkImage VulkanRenderer::currentSwapchainImage() const {
    if (currentImageIndex_ < swapchainImages_.size()) {
        return swapchainImages_[currentImageIndex_];
    }
    return VK_NULL_HANDLE;
}

// ============================================================================
// currentCommandBuffer
// ============================================================================
VkCommandBuffer VulkanRenderer::currentCommandBuffer() const {
    if (currentFrame_ < cmdBuffers_.size()) {
        return cmdBuffers_[currentFrame_];
    }
    return VK_NULL_HANDLE;
}

// ============================================================================
// Device info
// ============================================================================
std::string VulkanRenderer::getVendor() const {
    VkPhysicalDeviceProperties props{};
    vk.getPhysicalDeviceProperties(physDev_, &props);
    switch (props.vendorID) {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "Intel";
        case 0x1414: return "Microsoft";
        default: return "Unknown";
    }
}

std::string VulkanRenderer::getRenderer() const {
    VkPhysicalDeviceProperties props{};
    vk.getPhysicalDeviceProperties(physDev_, &props);
    return props.deviceName;
}

std::string VulkanRenderer::getVersion() const {
    VkPhysicalDeviceProperties props{};
    vk.getPhysicalDeviceProperties(physDev_, &props);
    char buf[128];
    sprintf_s(buf, "Vulkan %d.%d.%d",
        VK_VERSION_MAJOR(props.apiVersion),
        VK_VERSION_MINOR(props.apiVersion),
        VK_VERSION_PATCH(props.apiVersion));
    return buf;
}

// ============================================================================
// Screenshot
// ============================================================================
void VulkanRenderer::requestScreenshot(uint32_t w, uint32_t h) {
    screenshot_.pending = true;
    screenshot_.width = w;
    screenshot_.height = h;
    screenshot_.pixels.resize(w * h * 4);
    screenshotCopied_ = false;
}

std::vector<uint8_t> VulkanRenderer::takeScreenshot() {
    return std::move(screenshot_.pixels);
}

// ============================================================================
// ensureScreenshotStaging — create or recreate the staging buffer for readback
// ============================================================================
void VulkanRenderer::ensureScreenshotStaging(uint32_t w, uint32_t h) {
    VkDeviceSize needed = static_cast<VkDeviceSize>(w) * h * 4;
    if (screenshotStagingCreated_ && screenshotStaging_.size >= needed) return;

    if (screenshotStagingCreated_) {
        destroyBuffer(screenshotStaging_);
        screenshotStagingCreated_ = false;
    }

    screenshotStaging_ = createBuffer(
        needed,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (screenshotStaging_.buffer) {
        screenshotStagingCreated_ = true;
    }
}

// ============================================================================
// executeScreenshotCopy — record copy from swapchain image to staging buffer
// ============================================================================
void VulkanRenderer::executeScreenshotCopy(uint32_t w, uint32_t h) {
    if (!screenshotStagingCreated_ || !screenshotStaging_.buffer) return;
    if (screenshotCopied_) return;

    // Transition swapchain from COLOR_ATTACHMENT to TRANSFER_SRC
    cmdTransitionImage(currentSwapchainImage(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // Copy image to staging buffer
    VkImageResource swapchainRes{};
    swapchainRes.image = currentSwapchainImage();
    cmdCopyImageToBuffer(swapchainRes, screenshotStaging_, w, h);

    // Transition swapchain from TRANSFER_SRC directly to PRESENT_SRC
    cmdTransitionImage(currentSwapchainImage(),
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    screenshotCopied_ = true;
}

// ============================================================================
// finalizeScreenshot — wait for GPU, copy pixels from staging to output
// ============================================================================
void VulkanRenderer::finalizeScreenshot() {
    if (!screenshot_.pending) return;
    if (!screenshotStagingCreated_ || !screenshotStaging_.buffer) {
        screenshot_.pending = false;
        return;
    }

    // Wait for GPU to finish all work
    waitForIdle();

    // The buffer is persistently mapped (HOST_VISIBLE | HOST_COHERENT)
    // Copy BGRA pixels to output
    VkDeviceSize bytes = static_cast<VkDeviceSize>(screenshot_.width) * screenshot_.height * 4;
    if (screenshotStaging_.mapped && screenshot_.pixels.size() == bytes) {
        memcpy(screenshot_.pixels.data(), screenshotStaging_.mapped, static_cast<size_t>(bytes));
    }

    screenshot_.pending = false;
}

// ============================================================================
// loadInstanceFuncs — load instance-level functions manually
// ============================================================================
void VulkanRenderer::loadInstanceFuncs() {
    if (!instance_) return;
    vk.destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        vk.getInstanceProcAddr(instance_, "vkDestroyInstance"));
    vk.createDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vk.getInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    vk.destroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vk.getInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
    vk.createWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        vk.getInstanceProcAddr(instance_, "vkCreateWin32SurfaceKHR"));
    vk.destroySurfaceKHR = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vk.getInstanceProcAddr(instance_, "vkDestroySurfaceKHR"));
    vk.getPhysicalDeviceProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
        vk.getInstanceProcAddr(instance_, "vkGetPhysicalDeviceProperties"));
    vk.getPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
        vk.getInstanceProcAddr(instance_, "vkGetPhysicalDeviceMemoryProperties"));
    vk.getPhysicalDeviceQueueFamilyProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
        vk.getInstanceProcAddr(instance_, "vkGetPhysicalDeviceQueueFamilyProperties"));
    vk.getPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
        vk.getInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfaceSupportKHR"));
    vk.getPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        vk.getInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    vk.getPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
        vk.getInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    vk.getPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(
        vk.getInstanceProcAddr(instance_, "vkGetPhysicalDeviceSurfacePresentModesKHR"));
    vk.enumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        vk.getInstanceProcAddr(instance_, "vkEnumeratePhysicalDevices"));
    vk.enumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
        vk.getInstanceProcAddr(instance_, "vkEnumerateDeviceExtensionProperties"));
}

// ============================================================================
// loadDeviceFuncs — load device-level functions manually
// ============================================================================
void VulkanRenderer::loadDeviceFuncs() {
    if (!device_) return;
#define LOAD(member, vkname) if (!vk.member) vk.member = reinterpret_cast<PFN_vkname>(vk.getDeviceProcAddr(device_, #vkname));
    LOAD(destroyDevice, vkDestroyDevice)
    LOAD(getDeviceQueue, vkGetDeviceQueue)
    LOAD(createSwapchainKHR, vkCreateSwapchainKHR)
    LOAD(destroySwapchainKHR, vkDestroySwapchainKHR)
    LOAD(getSwapchainImagesKHR, vkGetSwapchainImagesKHR)
    LOAD(acquireNextImageKHR, vkAcquireNextImageKHR)
    LOAD(queuePresentKHR, vkQueuePresentKHR)
    LOAD(queueWaitIdle, vkQueueWaitIdle)
    LOAD(deviceWaitIdle, vkDeviceWaitIdle)
    LOAD(createCommandPool, vkCreateCommandPool)
    LOAD(destroyCommandPool, vkDestroyCommandPool)
    LOAD(allocateCommandBuffers, vkAllocateCommandBuffers)
    LOAD(freeCommandBuffers, vkFreeCommandBuffers)
    LOAD(beginCommandBuffer, vkBeginCommandBuffer)
    LOAD(endCommandBuffer, vkEndCommandBuffer)
    LOAD(cmdBeginRendering, vkCmdBeginRendering)
    LOAD(cmdEndRendering, vkCmdEndRendering)
    LOAD(cmdBindPipeline, vkCmdBindPipeline)
    LOAD(cmdSetViewport, vkCmdSetViewport)
    LOAD(cmdSetScissor, vkCmdSetScissor)
    LOAD(cmdDraw, vkCmdDraw)
    LOAD(cmdBlitImage, vkCmdBlitImage)
    LOAD(cmdCopyImageToBuffer, vkCmdCopyImageToBuffer)
    LOAD(cmdPipelineBarrier, vkCmdPipelineBarrier)
    LOAD(cmdBindVertexBuffers, vkCmdBindVertexBuffers)
    LOAD(cmdPushConstants, vkCmdPushConstants)
    LOAD(cmdBindDescriptorSets, vkCmdBindDescriptorSets)
    LOAD(createFence, vkCreateFence)
    LOAD(destroyFence, vkDestroyFence)
    LOAD(waitForFences, vkWaitForFences)
    LOAD(resetFences, vkResetFences)
    LOAD(createSemaphore, vkCreateSemaphore)
    LOAD(destroySemaphore, vkDestroySemaphore)
    LOAD(createImage, vkCreateImage)
    LOAD(destroyImage, vkDestroyImage)
    LOAD(getImageMemoryRequirements, vkGetImageMemoryRequirements)
    LOAD(allocateMemory, vkAllocateMemory)
    LOAD(freeMemory, vkFreeMemory)
    LOAD(bindImageMemory, vkBindImageMemory)
    LOAD(createImageView, vkCreateImageView)
    LOAD(destroyImageView, vkDestroyImageView)
    LOAD(createSampler, vkCreateSampler)
    LOAD(destroySampler, vkDestroySampler)
    LOAD(createBuffer, vkCreateBuffer)
    LOAD(destroyBuffer, vkDestroyBuffer)
    LOAD(getBufferMemoryRequirements, vkGetBufferMemoryRequirements)
    LOAD(bindBufferMemory, vkBindBufferMemory)
    LOAD(mapMemory, vkMapMemory)
    LOAD(unmapMemory, vkUnmapMemory)
    LOAD(createShaderModule, vkCreateShaderModule)
    LOAD(destroyShaderModule, vkDestroyShaderModule)
    LOAD(createPipelineLayout, vkCreatePipelineLayout)
    LOAD(destroyPipelineLayout, vkDestroyPipelineLayout)
    LOAD(createGraphicsPipelines, vkCreateGraphicsPipelines)
    LOAD(destroyPipeline, vkDestroyPipeline)
    LOAD(createDescriptorSetLayout, vkCreateDescriptorSetLayout)
    LOAD(destroyDescriptorSetLayout, vkDestroyDescriptorSetLayout)
    LOAD(createDescriptorPool, vkCreateDescriptorPool)
    LOAD(destroyDescriptorPool, vkDestroyDescriptorPool)
    LOAD(allocateDescriptorSets, vkAllocateDescriptorSets)
    LOAD(updateDescriptorSets, vkUpdateDescriptorSets)
    LOAD(queueSubmit, vkQueueSubmit)
    LOAD(cmdCopyBufferToImage, vkCmdCopyBufferToImage)
    LOAD(getImageSubresourceLayout, vkGetImageSubresourceLayout)
    LOAD(flushMappedMemoryRanges, vkFlushMappedMemoryRanges)
#undef LOAD
}

// ============================================================================
// loadPreset — create GPU resources for a compiled shader preset
// ============================================================================
// Minimal quad vertex shader (143 words vs 28304 from StageSplitter bloat)
static const uint32_t kMinimalVertSpv[] = {
    0x07230203, 0x00010300, 0x00280000, 0x00000017, 0x00000000, 0x00020011, 0x00000001, 0x0003000E, 0x00000000, 0x00000001, 0x0009000F, 0x00000000, 0x00000002, 0x6E69616D, 0x00000000, 0x00000011, 0x00000014, 0x00000009, 0x0000000F, 0x00030003, 0x0000000B, 0x00000001, 0x00050005, 0x00000009, 0x69736F70, 0x6E6F6974, 0x00000000, 0x00050005, 0x0000000F, 0x43786574, 0x64726F6F, 0x00000000, 0x000A0005, 0x00000014, 0x72746E65, 0x696F5079, 0x6150746E, 0x5F6D6172, 0x6E69616D, 0x745F762E, 0x6F437865, 0x0064726F, 0x00040005, 0x00000002, 0x6E69616D, 0x00000000, 0x00040047, 0x00000009, 0x0000001E, 0x00000000, 0x00040047, 0x0000000F, 0x0000001E, 0x00000001, 0x00040047, 0x00000011, 0x0000000B, 0x00000000, 0x00040047, 0x00000014, 0x0000001E, 0x00000000, 0x00020013, 0x00000001, 0x00030021, 0x00000003, 0x00000001, 0x00030016, 0x00000005, 0x00000020, 0x00040017, 0x00000006, 0x00000005, 0x00000002, 0x00040020, 0x00000008, 0x00000001, 0x00000006, 0x00040017, 0x0000000A, 0x00000005, 0x00000004, 0x0004002B, 0x00000005, 0x0000000C, 0x00000000, 0x0004002B, 0x00000005, 0x0000000D, 0x3F800000, 0x00040020, 0x00000010, 0x00000003, 0x0000000A, 0x00040020, 0x00000013, 0x00000003, 0x00000006, 0x0004003B, 0x00000008, 0x00000009, 0x00000001, 0x0004003B, 0x00000008, 0x0000000F, 0x00000001, 0x0004003B, 0x00000010, 0x00000011, 0x00000003, 0x0004003B, 0x00000013, 0x00000014, 0x00000003, 0x00050036, 0x00000001, 0x00000002, 0x00000000, 0x00000003, 0x000200F8, 0x00000004, 0x0004003D, 0x00000006, 0x00000007, 0x00000009, 0x00060050, 0x0000000A, 0x0000000B, 0x00000007, 0x0000000C, 0x0000000D, 0x0004003D, 0x00000006, 0x0000000E, 0x0000000F, 0x0003003E, 0x00000011, 0x0000000B, 0x0003003E, 0x00000014, 0x0000000E, 0x000100FD, 0x00010038
};
static const size_t kMinimalVertSpvWordCount = sizeof(kMinimalVertSpv) / sizeof(kMinimalVertSpv[0]);
bool VulkanRenderer::loadPreset(
    uint32_t width, uint32_t height,
    const std::vector<std::vector<uint32_t>>& vertSpvs,
    const std::vector<std::vector<uint32_t>>& fragSpvs,
    const std::vector<uint32_t>& samplerCounts,
    const std::vector<uint32_t>& pushSizes,
    const std::vector<VkFormat>& rtFormats,
    const std::vector<std::vector<float>>& pushDefaultsList,
    const std::vector<uint32_t>& uboSizes) {

    destroyPreset();
    preset_.outputWidth = width;
    preset_.outputHeight = height;

    size_t passCount = vertSpvs.size();
    preset_.passes.resize(passCount);

    FILE* logf = nullptr;
    fopen_s(&logf, "vk_preset.log", "w");
    if (logf) setvbuf(logf, nullptr, _IONBF, 0);
    if (logf) fprintf(logf, "loadPreset: %zu passes\n", passCount);

    for (size_t i = 0; i < passCount; i++) {
        auto& pass = preset_.passes[i];

        pass.vertSpirv = vertSpvs[i];
        pass.fragSpirv = fragSpvs[i];
        // Use minimal quad vertex shader instead of bloated StageSplitter output
        pass.vertSpirv.assign(kMinimalVertSpv, kMinimalVertSpv + kMinimalVertSpvWordCount);
        pass.samplerCount = samplerCounts[i];
        pass.samplerBindings.clear();
        for (uint32_t b = 0; b < pass.samplerCount; b++) {
            pass.samplerBindings.push_back(2 + b);
        }
        pass.pushConstantSize = pushSizes[i];
        if (i < pushDefaultsList.size()) pass.pushDefaults = pushDefaultsList[i];
        pass.uboSize = (i < uboSizes.size() && uboSizes[i] > 0) ? uboSizes[i] : sizeof(UniformData);

        if (logf) fprintf(logf, "\nPass %zu: samplers=%u push=%u vert=%zu frag=%zu\n",
            i, pass.samplerCount, pass.pushConstantSize, pass.vertSpirv.size(), pass.fragSpirv.size());

        // Validate SPIR-V magic
        if (pass.vertSpirv.empty() || pass.vertSpirv[0] != 0x07230203) {
            if (logf) fprintf(logf, "  BAD vert SPIR-V magic: 0x%08X\n", pass.vertSpirv.empty() ? 0 : pass.vertSpirv[0]);
        }
        if (pass.fragSpirv.empty() || pass.fragSpirv[0] != 0x07230203) {
            if (logf) fprintf(logf, "  BAD frag SPIR-V magic: 0x%08X\n", pass.fragSpirv.empty() ? 0 : pass.fragSpirv[0]);
        }

        // Create shader modules
        if (logf) { fprintf(logf, "  creating vert module (%zu words)...\n", pass.vertSpirv.size()); }
        pass.vertModule = createShaderModule(pass.vertSpirv);
        if (logf) { fprintf(logf, "  vert module = %p\n", (void*)pass.vertModule); }
        if (logf) { fprintf(logf, "  creating frag module (%zu words)...\n", pass.fragSpirv.size()); }
        pass.fragModule = createShaderModule(pass.fragSpirv);
        if (logf) { fprintf(logf, "  frag module = %p\n", (void*)pass.fragModule); }
        if (!pass.vertModule || !pass.fragModule) {
            if (logf) fprintf(logf, "  FAILED: shader modules (driver hang or error) — marking pass %zu as invalid\n", i);
            pass.valid = false;
            // Don't abort — continue loading remaining passes
            continue;
        }
        pass.valid = true;
        if (logf) fprintf(logf, "  shader modules OK\n");

        // Create descriptor set layout
        // Binding 0: UBO (MVP + sizes), Binding 2+: samplers
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(uboBinding);

        for (uint32_t b = 0; b < pass.samplerCount; b++) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = 2 + b;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(binding);
        }
        if (!bindings.empty()) {
            pass.descSetLayout = createDescriptorSetLayout(bindings);
        }

        // Create pipeline layout
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = pass.pushConstantSize > 0 ? pass.pushConstantSize : 64;

        std::vector<VkDescriptorSetLayout> layouts;
        if (pass.descSetLayout) layouts.push_back(pass.descSetLayout);
        std::vector<VkPushConstantRange> pushRanges = {pushRange};
        pass.pipelineLayout = createPipelineLayout(layouts, pushRanges);

        // Create pipeline
        VkFormat rtFormat = (i < rtFormats.size()) ? rtFormats[i] : swapchainFormat_;
        VkVertexInputBindingDescription vertBinding{};
        vertBinding.binding = 0;
        vertBinding.stride = sizeof(QuadVertex);
        vertBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertAttrs[2]{};
        vertAttrs[0].binding = 0;
        vertAttrs[0].location = 0;
        vertAttrs[0].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[0].offset = offsetof(QuadVertex, position);
        vertAttrs[1].binding = 0;
        vertAttrs[1].location = 1;
        vertAttrs[1].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[1].offset = offsetof(QuadVertex, texCoord);

        pass.pipeline = createGraphicsPipeline(
            pass.vertSpirv, pass.fragSpirv,
            pass.pipelineLayout, rtFormat,
            VK_FORMAT_UNDEFINED, false,
            vertBinding, vertAttrs);

        if (!pass.pipeline) {
            if (logf) fprintf(logf, "  FAILED: pipeline creation for pass %zu — marking invalid\n", i);
            pass.valid = false;
            continue;
        }

        if (logf) fprintf(logf, "  pipeline OK\n");

        // Create render target
        pass.width = width;
        pass.height = height;
        pass.renderTarget = createImage(width, height, rtFormat, true);

        // Create uniform buffer (for MVP matrix + sizes)
        pass.uniformBuffer = createBuffer(
            pass.uboSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Allocate and update descriptor set
        if (pass.descSetLayout) {
            pass.descriptorSet = allocateDescriptorSet(pass.descSetLayout);
            if (pass.descriptorSet && pass.uniformBuffer.buffer) {
                updateDescriptorSetUniform(pass.descriptorSet, 0,
                    pass.uniformBuffer.buffer, pass.uboSize);
            }
        }
    }

    // Create a linear sampler for all texture reads
    auto linearSampler = createSampler(
        VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    if (linearSampler.sampler) {
        preset_.samplers.push_back(linearSampler);
    }

    if (logf) { fprintf(logf, "\nloadPreset: SUCCESS (%zu samplers)\n", preset_.samplers.size()); fclose(logf); }
    return true;
}

// ============================================================================
// loadPresetReflection — create GPU resources with reflection-driven layouts
// ============================================================================
bool VulkanRenderer::loadPresetReflection(
    uint32_t width, uint32_t height,
    const std::vector<std::vector<uint32_t>>& vertSpvs,
    const std::vector<std::vector<uint32_t>>& fragSpvs,
    const std::vector<uint32_t>& pushSizes,
    const std::vector<VkFormat>& rtFormats,
    const std::vector<std::vector<float>>& pushDefaultsList,
    const std::vector<uint32_t>& uboSizes,
    const std::vector<std::vector<SamplerBindingInfo>>& perPassSamplerBindings) {

    destroyPreset();
    preset_.outputWidth = width;
    preset_.outputHeight = height;

    size_t passCount = vertSpvs.size();
    preset_.passes.resize(passCount);

    FILE* logf = nullptr;
    fopen_s(&logf, "vk_preset_reflect.log", "w");
    if (logf) fprintf(logf, "loadPresetReflection: %zu passes\n", passCount);

    for (size_t i = 0; i < passCount; i++) {
        auto& pass = preset_.passes[i];

        pass.vertSpirv = vertSpvs[i];
        pass.fragSpirv = fragSpvs[i];
        pass.pushConstantSize = pushSizes[i];
        if (i < pushDefaultsList.size()) pass.pushDefaults = pushDefaultsList[i];
        pass.uboSize = (i < uboSizes.size() && uboSizes[i] > 0) ? uboSizes[i] : sizeof(UniformData);

        const auto& samplerBindings = (i < perPassSamplerBindings.size()) ? perPassSamplerBindings[i] : std::vector<SamplerBindingInfo>{};
        pass.samplerCount = static_cast<uint32_t>(samplerBindings.size());
        pass.samplerBindings.clear();
        pass.samplerNames.clear();
        for (const auto& sb : samplerBindings) {
            pass.samplerBindings.push_back(sb.binding);
            pass.samplerNames.push_back(sb.samplerName);
        }

        if (logf) fprintf(logf, "\nPass %zu: samplers=%u push=%u vert=%zu frag=%zu\n",
            i, pass.samplerCount, pass.pushConstantSize, pass.vertSpirv.size(), pass.fragSpirv.size());

        // Create shader modules
        pass.vertModule = createShaderModule(pass.vertSpirv);
        pass.fragModule = createShaderModule(pass.fragSpirv);
        if (!pass.vertModule || !pass.fragModule) {
            if (logf) fprintf(logf, "  FAILED: shader modules\n");
            if (logf) fclose(logf);
            destroyPreset();
            return false;
        }
        if (logf) fprintf(logf, "  shader modules OK\n");

        // Create descriptor set layout from reflection bindings
        // Binding 0: UBO always; other bindings from reflection
        std::vector<VkDescriptorSetLayoutBinding> bindings;
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding = 0;
        uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings.push_back(uboBinding);

        for (const auto& sb : samplerBindings) {
            VkDescriptorSetLayoutBinding binding{};
            binding.binding = sb.binding;
            binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            binding.descriptorCount = 1;
            binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            bindings.push_back(binding);
            if (logf) fprintf(logf, "  sampler binding=%u name=%s\n", sb.binding, sb.samplerName.c_str());
        }
        if (!bindings.empty()) {
            pass.descSetLayout = createDescriptorSetLayout(bindings);
        }

        // Create pipeline layout
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.offset = 0;
        pushRange.size = pass.pushConstantSize > 0 ? pass.pushConstantSize : 64;

        std::vector<VkDescriptorSetLayout> layouts;
        if (pass.descSetLayout) layouts.push_back(pass.descSetLayout);
        std::vector<VkPushConstantRange> pushRanges = {pushRange};
        pass.pipelineLayout = createPipelineLayout(layouts, pushRanges);

        // Create pipeline
        VkFormat rtFormat = (i < rtFormats.size()) ? rtFormats[i] : swapchainFormat_;
        VkVertexInputBindingDescription vertBinding{};
        vertBinding.binding = 0;
        vertBinding.stride = sizeof(QuadVertex);
        vertBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription vertAttrs[2]{};
        vertAttrs[0].binding = 0;
        vertAttrs[0].location = 0;
        vertAttrs[0].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[0].offset = offsetof(QuadVertex, position);
        vertAttrs[1].binding = 0;
        vertAttrs[1].location = 1;
        vertAttrs[1].format = VK_FORMAT_R32G32_SFLOAT;
        vertAttrs[1].offset = offsetof(QuadVertex, texCoord);

        pass.pipeline = createGraphicsPipeline(
            pass.vertSpirv, pass.fragSpirv,
            pass.pipelineLayout, rtFormat,
            VK_FORMAT_UNDEFINED, false,
            vertBinding, vertAttrs);

        if (!pass.pipeline) {
            if (logf) fprintf(logf, "  FAILED: pipeline creation\n");
            if (logf) fclose(logf);
            destroyPreset();
            return false;
        }

        if (logf) fprintf(logf, "  pipeline OK\n");

        // Create render target
        pass.width = width;
        pass.height = height;
        pass.renderTarget = createImage(width, height, rtFormat, true);

        // Create uniform buffer
        pass.uniformBuffer = createBuffer(
            pass.uboSize,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        // Allocate and update descriptor set
        if (pass.descSetLayout) {
            pass.descriptorSet = allocateDescriptorSet(pass.descSetLayout);
            if (pass.descriptorSet && pass.uniformBuffer.buffer) {
                updateDescriptorSetUniform(pass.descriptorSet, 0,
                    pass.uniformBuffer.buffer, pass.uboSize);
            }
        }
    }

    // Create a linear sampler for all texture reads
    auto linearSampler = createSampler(
        VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    if (linearSampler.sampler) {
        preset_.samplers.push_back(linearSampler);
    }

    if (logf) { fprintf(logf, "\nloadPresetReflection: SUCCESS (%zu samplers)\n", preset_.samplers.size()); fclose(logf); }
    return true;
}

// ============================================================================
// blitAppContentOverPreset — blit GDI content on top of shader output
// ============================================================================
bool VulkanRenderer::blitAppContentOverPreset(uint32_t width, uint32_t height) {
    if (!frameActive_) return false;
    if (!uploadImage_.image) return false;

    // Transition swapchain from PRESENT_SRC back to TRANSFER_DST
    cmdTransitionImage(currentSwapchainImage(),
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // Transition uploadImage_ to TRANSFER_SRC for blit source
    cmdTransitionLayout(uploadImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    // Blit uploadImage_ (GDI content) on top of swapchain (shader output)
    VkImageBlit region{};
    region.srcOffsets[0] = { 0, 0, 0 };
    region.srcOffsets[1] = { static_cast<int32_t>(uploadImage_.width),
                             static_cast<int32_t>(uploadImage_.height), 1 };
    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.layerCount = 1;
    region.dstOffsets[0] = { 0, 0, 0 };
    region.dstOffsets[1] = { static_cast<int32_t>(swapchainExtent_.width),
                             static_cast<int32_t>(swapchainExtent_.height), 1 };
    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.layerCount = 1;

    vk.cmdBlitImage(cmdBuffers_[currentFrame_],
        uploadImage_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        currentSwapchainImage(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &region, VK_FILTER_LINEAR);

    // Transition uploadImage_ back to SHADER_READ_ONLY for next frame's shader access
    cmdTransitionLayout(uploadImage_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Transition swapchain to present
    cmdTransitionImage(currentSwapchainImage(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    return true;
}

// ============================================================================
// destroyPreset
// ============================================================================
void VulkanRenderer::destroyPreset() {
    for (auto& pass : preset_.passes) {
        if (pass.pipeline) destroyPipeline(pass.pipeline);
        if (pass.pipelineLayout) destroyPipelineLayout(pass.pipelineLayout);
        if (pass.descSetLayout) destroyDescriptorSetLayout(pass.descSetLayout);
        if (pass.vertModule) destroyShaderModule(pass.vertModule);
        if (pass.fragModule) destroyShaderModule(pass.fragModule);
        if (pass.renderTarget.image) destroyImage(pass.renderTarget);
        if (pass.uniformBuffer.buffer) destroyBuffer(pass.uniformBuffer);
    }
    for (auto& s : preset_.samplers) {
        destroySampler(s);
    }
    for (auto& img : preset_.images) {
        destroyImage(img);
    }
    preset_ = {};
}

// ============================================================================
// renderPreset — execute all passes of the loaded preset
// ============================================================================
bool VulkanRenderer::renderPreset(uint32_t width, uint32_t height) {
    if (!preset_.valid()) { OutputDebugStringA("[VK] renderPreset FAIL: preset not valid\n"); return false; }
    if (!frameActive_) { OutputDebugStringA("[VK] renderPreset FAIL: frame not active\n"); return false; }

    VkImageView sourceView = uploadImage_.view;
    if (!sourceView) { OutputDebugStringA("[VK] renderPreset FAIL: sourceView null\n"); return false; }

    char buf[256];
    sprintf_s(buf, "[VK] renderPreset: %zu passes, src=%dx%d\n", preset_.passes.size(), uploadImage_.width, uploadImage_.height);
    OutputDebugStringA(buf);

    for (size_t i = 0; i < preset_.passes.size(); i++) {
        auto& pass = preset_.passes[i];

        // Skip passes that failed shader compilation or pipeline creation
        if (!pass.valid || !pass.pipeline) {
            OutputDebugStringA("[VK] renderPreset: skipping invalid pass\n");
            continue;
        }

        // Determine render target (last pass goes to swapchain)
        bool isLastPass = (i == preset_.passes.size() - 1);
        VkImageView rtView = isLastPass ? currentSwapchainView() : pass.renderTarget.view;
        uint32_t rtW = isLastPass ? width : pass.width;
        uint32_t rtH = isLastPass ? height : pass.height;

        // Transition render target to color attachment
        if (isLastPass) {
            // Last pass renders to swapchain — transition from present (or undefined) to color attachment
            cmdTransitionImage(currentSwapchainImage(),
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        } else if (pass.renderTarget.image) {
            cmdTransitionImage(pass.renderTarget.image,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }

        // uploadImage_ is already in SHADER_READ_ONLY from uploadTextureToImage
        // (layout tracked via cmdTransitionLayout). No transition needed here.

        // Update descriptor set for this pass with correct textures
        if (pass.descriptorSet) {
            // Build name→image map for this pass from all previous passes
            // "Source" = uploaded GDI image for pass 0, previous pass's RT for others
            // "XxxPass" = pass that produced that output
            // Feedback textures = current frame output (first frame = black)
            for (size_t s = 0; s < pass.samplerCount && s < pass.samplerBindings.size(); s++) {
                uint32_t binding = pass.samplerBindings[s];
                const std::string& name = (s < pass.samplerNames.size()) ? pass.samplerNames[s] : "";
                VkImageView imageView = VK_NULL_HANDLE;

                if (name == "Source") {
                    // Source: pass 0 gets uploadImage_, later passes get prev pass RT
                    imageView = (i == 0) ? sourceView : preset_.passes[i - 1].renderTarget.view;
                } else if (name == "PastSampler") {
                    // PastSampler = feedback (previous frame) — use prev pass RT for now
                    imageView = (i > 0) ? preset_.passes[i - 1].renderTarget.view : sourceView;
                } else {
                    // Named pass output: find the pass that produces this name
                    for (size_t p = 0; p < preset_.passes.size(); p++) {
                        if (p == i) continue; // don't bind own output as input
                        // Check if this pass's name matches (simplified: match partial name)
                        const auto& otherPass = preset_.passes[p];
                        if (!otherPass.renderTarget.view) continue;
                        // The pass output name is embedded in the sampler name
                        // e.g., sampler "colortools_and_ntsc_pass" → pass[0] output
                        // We use a simple substring check
                        if (!name.empty() && otherPass.renderTarget.view) {
                            // Try exact match against known pass outputs
                            // Pass outputs are identified by their position in the pipeline
                            // For Mega Bezel, pass names follow: pass[N] output is the render target of pass N
                            // We check if the name matches any pass's sampler name list
                            bool matched = false;
                            for (size_t q = 0; q < p && q < preset_.passes.size(); q++) {
                                const auto& earlierPass = preset_.passes[q];
                                for (const auto& eName : earlierPass.samplerNames) {
                                    if (eName == name) { matched = true; break; }
                                }
                                if (matched) break;
                            }
                            // Use pass p's render target if it looks like it produces this output
                            // Simple heuristic: if the name contains a pass identifier, use that pass's RT
                            imageView = otherPass.renderTarget.view;
                            matched = true; // Use first available RT as fallback
                            break;
                        }
                    }
                }

                // Fallback: use source image
                if (!imageView) {
                    imageView = sourceView;
                }

                if (imageView) {
                    updateDescriptorSetTexture(pass.descriptorSet, binding,
                        imageView, preset_.samplers[0].sampler);
                }
            }
        }

        // Begin rendering
        VkClearValue clear{};
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        cmdBeginRendering(rtView, rtW, rtH, clear);

        // Bind pipeline
        cmdBindPipeline(pass.pipeline, pass.pipelineLayout);

        // Set viewport and scissor
        cmdSetViewport(rtW, rtH);
        cmdSetScissor(rtW, rtH);

        // Update uniform buffer with current MVP and size data
        // UBO layout: mat4 MVP (64) + vec4 OutputSize (16) + vec4 OriginalSize (16) + vec4 SourceSize (16)
        // Minimum 112 bytes for CRT shaders; stock.slang only needs 64 but extra data is harmless
        struct PresetUBO {
            float mvp[16];
            float outputSize[4];
            float originalSize[4];
            float sourceSize[4];
        } ubo{};
        // Identity MVP — quad vertices are already in clip space [-1,1]
        ubo.mvp[0] =  1.0f;
        ubo.mvp[5] =  1.0f;
        ubo.mvp[10] = 1.0f;
        ubo.mvp[15] = 1.0f;
        // OutputSize = (width, height, 1/width, 1/height)
        ubo.outputSize[0] = static_cast<float>(rtW);
        ubo.outputSize[1] = static_cast<float>(rtH);
        ubo.outputSize[2] = 1.0f / static_cast<float>(rtW);
        ubo.outputSize[3] = 1.0f / static_cast<float>(rtH);
        // OriginalSize = (original_width, original_height, 1/orig_w, 1/orig_h)
        ubo.originalSize[0] = static_cast<float>(width);
        ubo.originalSize[1] = static_cast<float>(height);
        ubo.originalSize[2] = 1.0f / static_cast<float>(width);
        ubo.originalSize[3] = 1.0f / static_cast<float>(height);
        // SourceSize = input texture dimensions
        // Pass 0: source = uploadImage_ (original UI texture)
        // Pass 1: source = pass[0].renderTarget
        // Pass 2: source = uploadImage_ (via Reference binding) or pass[0].renderTarget
        float srcW, srcH;
        if (i == 0) {
            // Source is the uploaded GDI texture
            srcW = static_cast<float>(uploadImage_.width);
            srcH = static_cast<float>(uploadImage_.height);
        } else {
            // Source is the previous pass's render target (same dimensions as output)
            srcW = static_cast<float>(rtW);
            srcH = static_cast<float>(rtH);
        }
        ubo.sourceSize[0] = srcW;
        ubo.sourceSize[1] = srcH;
        ubo.sourceSize[2] = 1.0f / srcW;
        ubo.sourceSize[3] = 1.0f / srcH;

        // Upload UBO to GPU buffer
        uploadBuffer(pass.uniformBuffer, &ubo, sizeof(ubo));

        // Push constants: use CRT parameter defaults from preset, padded to pushConstantSize
        std::vector<uint8_t> pushData(pass.pushConstantSize > 0 ? pass.pushConstantSize : 0, 0);
        if (!pass.pushDefaults.empty() && pass.pushConstantSize > 0) {
            size_t srcBytes = pass.pushDefaults.size() * sizeof(float);
            size_t copyBytes = (srcBytes < static_cast<size_t>(pass.pushConstantSize))
                             ? srcBytes : static_cast<size_t>(pass.pushConstantSize);
            memcpy(pushData.data(), pass.pushDefaults.data(), copyBytes);
        }
        if (!pushData.empty()) {
            cmdPushConstants(pass.pipelineLayout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, pass.pushConstantSize, pushData.data());
        }

        // Bind descriptor set
        if (pass.descriptorSet) {
            cmdBindDescriptorSet(pass.pipelineLayout, pass.descriptorSet);
        }

        // Draw full-screen quad
        drawFullScreenQuad();

        // End rendering
        cmdEndRendering();

        // Transition render target to shader read for next pass
        if (!isLastPass && pass.renderTarget.image) {
            cmdTransitionImage(pass.renderTarget.image,
                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        } else if (isLastPass) {
            // Last pass wrote to swapchain
            if (screenshot_.pending && screenshot_.width > 0 && screenshot_.height > 0 && !screenshotCopied_) {
                // Screenshot requested: copy swapchain to staging buffer, transition to PRESENT_SRC
                ensureScreenshotStaging(screenshot_.width, screenshot_.height);
                executeScreenshotCopy(screenshot_.width, screenshot_.height);
                // Already transitioned to PRESENT_SRC in executeScreenshotCopy
            } else {
                // Normal: transition to present
                cmdTransitionImage(currentSwapchainImage(),
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            }
        }
    }

    return true;
}
