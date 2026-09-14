#pragma once
// ============================================================================
// vulkan_renderer.h — Vulkan 1.3 renderer with Dynamic Rendering
//
// Replaces the entire OpenGL backend. Handles:
//   - VkInstance, VkSurfaceKHR, VkPhysicalDevice, VkDevice, VkQueue
//   - VkSwapchainKHR with automatic recreation
//   - VkCommandPool, VkCommandBuffer per frame
//   - VkFence + VkSemaphore synchronization (triple-buffered)
//   - VkImage, VkImageView, VkDeviceMemory for textures/render targets
//   - VkSampler with filtering/wrap/border
//   - VkShaderModule, VkPipelineLayout, VkDescriptorSetLayout
//   - VkPipeline with VK_KHR_dynamic_rendering (no VkRenderPass)
//   - VkDescriptorPool, VkDescriptorSet with per-frame ring
//   - VkBuffer for vertex/uniform data
//   - Full-screen quad rendering for shader passes
//   - Screenshot readback via VkImage + staging buffer
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define VK_NO_PROTOTYPES
#include "vulkan-headers/vulkan.h"
#include "vulkan-headers/vulkan_win32.h"

#include <cstdint>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include <functional>
#include <optional>
#include <span>
#include <mutex>

// ============================================================================
// Vulkan function pointers loaded from vulkan-1.dll
// ============================================================================
namespace vkfn {
void loadGlobal();
void loadInstance(VkInstance inst);
void loadDevice(VkDevice dev);
}

// ============================================================================
// Forward declarations
// ============================================================================

// ============================================================================
// Vulkan function pointers loaded from vulkan-1.dll (instance-level + device-level)
// ============================================================================
struct VkFuncs {
    HMODULE vkModule = nullptr;
    PFN_vkGetInstanceProcAddr getInstanceProcAddr = nullptr;
    PFN_vkGetDeviceProcAddr getDeviceProcAddr = nullptr;
    // Instance-level
    PFN_vkCreateInstance createInstance = nullptr;
    PFN_vkDestroyInstance destroyInstance = nullptr;
    PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices = nullptr;
    PFN_vkGetPhysicalDeviceProperties getPhysicalDeviceProperties = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkGetPhysicalDeviceQueueFamilyProperties getPhysicalDeviceQueueFamilyProperties = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceSupportKHR getPhysicalDeviceSurfaceSupportKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR getPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR getPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR getPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkCreateWin32SurfaceKHR createWin32SurfaceKHR = nullptr;
    PFN_vkDestroySurfaceKHR destroySurfaceKHR = nullptr;
    PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT = nullptr;
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT = nullptr;
    PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties = nullptr;
    // Device-level
    PFN_vkCreateDevice createDevice = nullptr;
    PFN_vkDestroyDevice destroyDevice = nullptr;
    PFN_vkGetDeviceQueue getDeviceQueue = nullptr;
    PFN_vkCreateSwapchainKHR createSwapchainKHR = nullptr;
    PFN_vkDestroySwapchainKHR destroySwapchainKHR = nullptr;
    PFN_vkGetSwapchainImagesKHR getSwapchainImagesKHR = nullptr;
    PFN_vkAcquireNextImageKHR acquireNextImageKHR = nullptr;
    PFN_vkQueuePresentKHR queuePresentKHR = nullptr;
    PFN_vkQueueWaitIdle queueWaitIdle = nullptr;
    PFN_vkDeviceWaitIdle deviceWaitIdle = nullptr;
    PFN_vkCreateCommandPool createCommandPool = nullptr;
    PFN_vkDestroyCommandPool destroyCommandPool = nullptr;
    PFN_vkAllocateCommandBuffers allocateCommandBuffers = nullptr;
    PFN_vkFreeCommandBuffers freeCommandBuffers = nullptr;
    PFN_vkBeginCommandBuffer beginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer endCommandBuffer = nullptr;
    PFN_vkCmdBeginRendering cmdBeginRendering = nullptr;
    PFN_vkCmdEndRendering cmdEndRendering = nullptr;
    PFN_vkCmdBindPipeline cmdBindPipeline = nullptr;
    PFN_vkCmdSetViewport cmdSetViewport = nullptr;
    PFN_vkCmdSetScissor cmdSetScissor = nullptr;
    PFN_vkCmdDraw cmdDraw = nullptr;
    PFN_vkCmdBlitImage cmdBlitImage = nullptr;
    PFN_vkCmdCopyImageToBuffer cmdCopyImageToBuffer = nullptr;
    PFN_vkCmdPipelineBarrier cmdPipelineBarrier = nullptr;
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers = nullptr;
    PFN_vkCmdPushConstants cmdPushConstants = nullptr;
    PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets = nullptr;
    PFN_vkCreateFence createFence = nullptr;
    PFN_vkDestroyFence destroyFence = nullptr;
    PFN_vkWaitForFences waitForFences = nullptr;
    PFN_vkResetFences resetFences = nullptr;
    PFN_vkCreateSemaphore createSemaphore = nullptr;
    PFN_vkDestroySemaphore destroySemaphore = nullptr;
    PFN_vkCreateImage createImage = nullptr;
    PFN_vkDestroyImage destroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements getImageMemoryRequirements = nullptr;
    PFN_vkAllocateMemory allocateMemory = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    PFN_vkBindImageMemory bindImageMemory = nullptr;
    PFN_vkCreateImageView createImageView = nullptr;
    PFN_vkDestroyImageView destroyImageView = nullptr;
    PFN_vkCreateSampler createSampler = nullptr;
    PFN_vkDestroySampler destroySampler = nullptr;
    PFN_vkCreateBuffer createBuffer = nullptr;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkGetBufferMemoryRequirements getBufferMemoryRequirements = nullptr;
    PFN_vkBindBufferMemory bindBufferMemory = nullptr;
    PFN_vkMapMemory mapMemory = nullptr;
    PFN_vkUnmapMemory unmapMemory = nullptr;
    PFN_vkCreateShaderModule createShaderModule = nullptr;
    PFN_vkDestroyShaderModule destroyShaderModule = nullptr;
    PFN_vkCreatePipelineLayout createPipelineLayout = nullptr;
    PFN_vkDestroyPipelineLayout destroyPipelineLayout = nullptr;
    PFN_vkCreateGraphicsPipelines createGraphicsPipelines = nullptr;
    PFN_vkDestroyPipeline destroyPipeline = nullptr;
    PFN_vkCreateDescriptorSetLayout createDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorSetLayout destroyDescriptorSetLayout = nullptr;
    PFN_vkCreateDescriptorPool createDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorPool destroyDescriptorPool = nullptr;
    PFN_vkAllocateDescriptorSets allocateDescriptorSets = nullptr;
    PFN_vkUpdateDescriptorSets updateDescriptorSets = nullptr;
    PFN_vkQueueSubmit queueSubmit = nullptr;
    PFN_vkCmdCopyBufferToImage cmdCopyBufferToImage = nullptr;
    PFN_vkGetImageSubresourceLayout getImageSubresourceLayout = nullptr;
    PFN_vkFlushMappedMemoryRanges flushMappedMemoryRanges = nullptr;
};

// ============================================================================
// Configuration
// ============================================================================
static constexpr uint32_t kMaxFramesInFlight = 2;
static constexpr uint32_t kMaxDescriptorSets = 4096;
static constexpr uint32_t kMaxSamplers = 256;
static constexpr uint32_t kMaxUniformBuffers = 512;
static constexpr uint32_t kMaxImages = 1024;

// ============================================================================
// VkImage + memory wrapper
// ============================================================================
struct VkImageResource {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    bool isRenderTarget = false;
    VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

// ============================================================================
// VkSampler wrapper
// ============================================================================
struct VkSamplerResource {
    VkSampler sampler = VK_NULL_HANDLE;
};

// ============================================================================
// VkBuffer wrapper (vertex, uniform, staging)
// ============================================================================
struct VkBufferResource {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    void* mapped = nullptr;
};

// ============================================================================
// Pipeline state key for caching
// ============================================================================
struct PipelineKey {
    uint32_t vertModuleHash;
    uint32_t fragModuleHash;
    VkFormat colorFormat;
    VkFormat depthFormat;
    bool hasDepth;
    bool hasBlend;
    bool operator==(const PipelineKey& o) const;
};

// ============================================================================
// Per-pass pipeline resources
// ============================================================================
struct PassPipeline {
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
};

// ============================================================================
// Full-screen quad vertex
// ============================================================================
struct QuadVertex {
    float position[2];
    float texCoord[2];
};

// ============================================================================
// Uniform buffer data (std140 layout)
// ============================================================================
struct UniformData {
    float mvp[16];
    float sourceSize[4];
    float originalSize[4];
    float outputSize[4];
    float finalViewportSize[4];
    uint32_t frameCount;
    uint32_t frameDirection;
    float padding[2];
};

// ============================================================================
// Screenshot request
// ============================================================================
struct ScreenshotRequest {
    bool pending = false;
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels; // BGRA output
};

// ============================================================================
// Vulkan validation message
// ============================================================================
struct ValidationMessage {
    uint32_t severity;
    uint32_t type;
    std::string message;
};

// ============================================================================
// VulkanRenderer — the complete Vulkan 1.3 rendering backend
// ============================================================================
class VulkanRenderer {
public:
    VulkanRenderer();
    ~VulkanRenderer();

    // ---- Lifecycle ----
    bool initialize(HWND hwnd, uint32_t width, uint32_t height);
    void shutdown();
    bool isInitialized() const { return initialized_; }

    // ---- Validation ----
    bool enableValidation();                   // call BEFORE initialize()
    bool isValidationEnabled() const { return validationEnabled_; }
    bool isValidationAvailable() const { return validationAvailable_; }
    std::vector<ValidationMessage> takeValidationMessages(); // drain & return
    void clearValidationMessages();
    void addValidationMessage(ValidationMessage msg);
    uint32_t validationErrorCount() const;
    uint32_t validationWarningCount() const;
    uint32_t validationCriticalCount() const;

    // ---- Swapchain ----
    void resize(uint32_t width, uint32_t height);

    // ---- Frame ----
    bool beginFrame(uint32_t width, uint32_t height);
    void endFrame();
    void present();
    void waitForIdle();

    // ---- Resource creation ----
    VkImageResource createImage(uint32_t w, uint32_t h, VkFormat fmt,
                                bool renderTarget, bool cpuReadable = false);
    void destroyImage(VkImageResource& img);

    VkSamplerResource createSampler(VkFilter minFilter, VkFilter magFilter,
                                     VkSamplerAddressMode wrapS,
                                     VkSamplerAddressMode wrapT,
                                     float borderColorR = 0, float borderColorG = 0,
                                     float borderColorB = 0, float borderColorA = 1);
    void destroySampler(VkSamplerResource& s);

    VkBufferResource createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                   VkMemoryPropertyFlags props);
    void destroyBuffer(VkBufferResource& buf);
    void uploadBuffer(VkBufferResource& dst, const void* data, VkDeviceSize size);

    // ---- Descriptor management ----
    VkDescriptorSetLayout createDescriptorSetLayout(
        std::span<const VkDescriptorSetLayoutBinding> bindings);
    void destroyDescriptorSetLayout(VkDescriptorSetLayout layout);

    VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout layout);
    void updateDescriptorSetTexture(VkDescriptorSet set, uint32_t binding,
                                     VkImageView view, VkSampler sampler,
                                     VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    void updateDescriptorSetUniform(VkDescriptorSet set, uint32_t binding,
                                     VkBuffer buffer, VkDeviceSize range);

    // ---- Pipeline creation ----
    VkPipelineLayout createPipelineLayout(
        std::span<const VkDescriptorSetLayout> descLayouts,
        std::span<const VkPushConstantRange> pushRanges = {});
    void destroyPipelineLayout(VkPipelineLayout layout);

    VkPipeline createGraphicsPipeline(
        std::span<const uint32_t> vertSpirv,
        std::span<const uint32_t> fragSpirv,
        VkPipelineLayout layout,
        VkFormat colorFormat,
        VkFormat depthFormat = VK_FORMAT_UNDEFINED,
        bool hasBlend = false,
        VkVertexInputBindingDescription vertBinding = {},
        std::span<const VkVertexInputAttributeDescription> vertAttrs = {});
    void destroyPipeline(VkPipeline pipeline);

    // ---- Shader module creation ----
    VkShaderModule createShaderModule(std::span<const uint32_t> spirv);
    void destroyShaderModule(VkShaderModule mod);

    // ---- Render commands (record into current command buffer) ----
    void cmdBeginRendering(VkImageView colorTarget, uint32_t w, uint32_t h,
                           VkClearValue clear = {});
    void cmdEndRendering();

    void cmdBindPipeline(VkPipeline pipeline, VkPipelineLayout layout);
    void cmdBindDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set, uint32_t setIdx = 0);
    void cmdBindVertexBuffer(VkBufferResource& buf);
    void cmdSetViewport(uint32_t w, uint32_t h);
    void cmdSetScissor(uint32_t w, uint32_t h);
    void cmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stage,
                          uint32_t offset, uint32_t size, const void* data);
    void cmdDraw(uint32_t vertexCount, uint32_t firstVertex = 0);
    void cmdBlitImage(VkImageResource& src, VkImageResource& dst,
                      uint32_t srcW, uint32_t srcH, uint32_t dstW, uint32_t dstH);
    void cmdCopyImageToBuffer(VkImageResource& src, VkBufferResource& dst,
                               uint32_t w, uint32_t h);
    void cmdCopyBufferToImage(VkBufferResource& src, VkImageResource& dst,
                               uint32_t w, uint32_t h);
    void cmdTransitionLayout(VkImageResource& img, VkImageLayout newLayout);
    void cmdTransitionImage(VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout,
                            VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});

    // ---- Full-screen quad ----
    void drawFullScreenQuad();

    // ---- Swapchain image access ----
    VkImageView currentSwapchainView() const;
    VkImage currentSwapchainImage() const;
    uint32_t currentImageIndex() const { return currentImageIndex_; }
    VkFormat swapchainFormat() const { return swapchainFormat_; }
    uint32_t swapchainWidth() const { return swapchainExtent_.width; }
    uint32_t swapchainHeight() const { return swapchainExtent_.height; }

    // ---- Device info ----
    std::string getVendor() const;
    std::string getRenderer() const;
    std::string getVersion() const;

    // ---- Screenshot ----
    void requestScreenshot(uint32_t w, uint32_t h);
    bool hasScreenshot() const { return screenshot_.pending == false && !screenshot_.pixels.empty(); }
    std::vector<uint8_t> takeScreenshot();
    void finalizeScreenshot();
    void recordScreenshotCopy();
    VkCommandBuffer currentCommandBuffer() const;

    // ---- Texture upload (GDI bitmap → swapchain blit) ----
    bool uploadTexture(const void* bgraPixels, uint32_t w, uint32_t h);
    bool uploadTextureToImage(const void* bgraPixels, uint32_t w, uint32_t h, bool blitToSwapchain);

    // ---- Source image access (for generic render loop) ----
    VkImageView sourceImageView() const { return uploadImage_.view; }
    VkImageResource& sourceImageResource() { return uploadImage_; }

    // ---- Shader preset rendering ----
    struct PresetPassResources {
        VkShaderModule vertModule = VK_NULL_HANDLE;
        VkShaderModule fragModule = VK_NULL_HANDLE;
        VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkImageResource renderTarget;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkBufferResource uniformBuffer;
        std::vector<uint32_t> vertSpirv;
        std::vector<uint32_t> fragSpirv;
        std::vector<float> pushDefaults;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t samplerCount = 0;
        uint32_t pushConstantSize = 0;
        uint32_t uboSize = 0;
        std::vector<uint32_t> samplerBindings;
        std::vector<std::string> samplerNames;
        bool valid = false;
    };

    struct LoadedPreset {
        std::vector<PresetPassResources> passes;
        std::vector<VkSamplerResource> samplers;
        std::vector<VkImageResource> images;
        uint32_t outputWidth = 0;
        uint32_t outputHeight = 0;
        bool valid() const { return !passes.empty(); }
    };

    bool loadPreset(uint32_t width, uint32_t height,
                    const std::vector<std::vector<uint32_t>>& vertSpvs,
                    const std::vector<std::vector<uint32_t>>& fragSpvs,
                    const std::vector<uint32_t>& samplerCounts,
                    const std::vector<uint32_t>& pushSizes,
                    const std::vector<VkFormat>& rtFormats,
                    const std::vector<std::vector<float>>& pushDefaults,
                    const std::vector<uint32_t>& uboSizes);

    struct SamplerBindingInfo {
        uint32_t binding = 0;
        std::string samplerName;
    };

    bool loadPresetReflection(uint32_t width, uint32_t height,
                    const std::vector<std::vector<uint32_t>>& vertSpvs,
                    const std::vector<std::vector<uint32_t>>& fragSpvs,
                    const std::vector<uint32_t>& pushSizes,
                    const std::vector<VkFormat>& rtFormats,
                    const std::vector<std::vector<float>>& pushDefaults,
                    const std::vector<uint32_t>& uboSizes,
                    const std::vector<std::vector<SamplerBindingInfo>>& perPassSamplerBindings);

    void destroyPreset();
    bool renderPreset(uint32_t width, uint32_t height);
    bool blitAppContentOverPreset(uint32_t width, uint32_t height);
    const LoadedPreset& currentPreset() const { return preset_; }

public: // Internal module state shared by the decomposed Vulkan implementation.
    // ---- Vulkan function pointers ----
    VkFuncs vk;

    // ---- Init helpers ----
    bool createInstance();
    bool createSurface(HWND hwnd);
    bool pickPhysicalDevice();
    bool createLogicalDevice();
    void loadInstanceFuncs();
    void loadDeviceFuncs();
    bool createSwapchain(uint32_t w, uint32_t h);
    bool createSyncObjects();
    bool createCommandPool();
    bool allocateCommandBuffers();
    bool createDescriptorPool();
    bool createQuadBuffer();

    // ---- Swapchain helpers ----
    void destroySwapchain();
    VkSwapchainKHR oldSwapchain_ = VK_NULL_HANDLE;

    // ---- Memory ----
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    bool allocateImageMemory(VkImage image, VkMemoryPropertyFlags props, VkDeviceMemory* outMem);
    bool transitionImageLayoutImmediate(VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout);

    // ---- State ----
    bool initialized_ = false;
    HWND hwnd_ = nullptr;

    // Vulkan core
    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physDev_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    uint32_t graphicsFamily_ = 0;
    uint32_t presentFamily_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {};

    // Command buffers
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, kMaxFramesInFlight> cmdBuffers_{};
    uint32_t currentFrame_ = 0;
    uint32_t currentImageIndex_ = 0;
    bool frameActive_ = false;

    // Synchronization
    std::array<VkSemaphore, kMaxFramesInFlight> imageAvailableSem_{};
    std::array<VkSemaphore, kMaxFramesInFlight> renderFinishedSem_{};
    std::array<VkFence, kMaxFramesInFlight> inFlightFences_{};
    bool syncCreated_ = false;

    // Descriptor pool
    VkDescriptorPool descPool_ = VK_NULL_HANDLE;

    // Full-screen quad
    VkBufferResource quadVbo_{};
    bool quadCreated_ = false;

    // Screenshot
    ScreenshotRequest screenshot_;
    VkBufferResource screenshotStaging_{};
    bool screenshotStagingCreated_ = false;
    bool screenshotCopied_ = false;
    void ensureScreenshotStaging(uint32_t w, uint32_t h);
    void executeScreenshotCopy(uint32_t w, uint32_t h);

    // Upload texture (for GDI → Vulkan)
    VkImageResource uploadImage_{};
    VkBufferResource uploadStaging_{};
    bool uploadCreated_ = false;

    // Shader preset
    LoadedPreset preset_{};
    uint32_t presetFrameCount_ = 0;

    // Dynamic rendering
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeat_{};
    VkPhysicalDeviceVulkan13Features vk13Features_{};

    // Validation
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    bool requestValidation_ = false;
    bool validationEnabled_ = false;
    bool validationAvailable_ = false;
    std::vector<ValidationMessage> validationMessages_;
    mutable std::mutex validationMutex_;

    // DLL handle (g_vkModule is the global from vk_globals.hpp)

    // Shader thread lifecycle — prevents use-after-free on shutdown
    std::atomic<int> pendingShaderThreads_{0};
    std::atomic<bool> shutdownRequested_{false};
};
