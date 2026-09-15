// ============================================================================
// vulkan_renderer.cpp — Orchestrator for decomposed Vulkan modules
// ============================================================================

#include "vulkan_renderer.h"

#include "renderer_vk/vk/vk_globals.hpp"
#include "renderer_vk/vk/instance/VkInstance.hpp"
#include "renderer_vk/vk/device/VkDevice.hpp"
#include "renderer_vk/vk/swapchain/VkSwapchain.hpp"
#include "renderer_vk/vk/sync/VkSync.hpp"
#include "renderer_vk/vk/command/VkCommand.hpp"
#include "renderer_vk/vk/descriptor/VkDescriptor.hpp"
#include "renderer_vk/vk/buffer/VkBuffer.hpp"
#include "renderer_vk/vk/cmd/VkCmdRecording.hpp"
#include "renderer_vk/vk/screenshot/VkScreenshot.hpp"
#include "renderer_vk/vk/upload/VkUploadTexture.hpp"
#include "renderer_vk/vk/preset/VkPreset.hpp"
#include "renderer_vk/vk/validation/VkValidation.hpp"
#include "renderer_vk/vk/image/VkImage.hpp"
#include "renderer_vk/vk/sampler/VkSampler.hpp"
#include "renderer_vk/vk/pipeline/VkPipeline.hpp"
#include "renderer_vk/vk/memory/VkMemory.hpp"

#include <cassert>
#include <cstring>
#include <algorithm>

struct VkCtx { VulkanRenderer& r; };

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
// Validation helpers
// ============================================================================
bool VulkanRenderer::enableValidation() {
    if (initialized_) return false;
    requestValidation_ = true;
    return true;
}

void VulkanRenderer::addValidationMessage(ValidationMessage msg) {
    std::lock_guard<std::mutex> lock(validationMutex_);
    if (validationMessages_.size() < 5000) {
        validationMessages_.push_back(std::move(msg));
    }
}

std::vector<ValidationMessage> VulkanRenderer::takeValidationMessages() {
    std::lock_guard<std::mutex> lock(validationMutex_);
    return std::move(validationMessages_);
}

void VulkanRenderer::clearValidationMessages() {
    std::lock_guard<std::mutex> lock(validationMutex_);
    validationMessages_.clear();
}

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
    VkCtx ctx{*this};

    // Pre-check: detect broken implicit layers BEFORE loading Vulkan.
    // VK_LAYER_AMD_switchable_graphics (DriverStore) and VK_LAYER_MEDIASDK_HOOK
    // (registry) wrap VkDevice handles incorrectly, causing
    // __fastfail(STATUS_STACK_BUFFER_OVERRUN) on any device-level call.
    {
        bool brokenLayerDetected = false;

        // Check HKLM registry for MEDIASDK_HOOK and switchable_graphics
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "SOFTWARE\\Khronos\\Vulkan\\ImplicitLayers",
                0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD idx = 0;
            char valueName[512];
            DWORD valueNameSize;
            while (true) {
                valueNameSize = sizeof(valueName);
                LONG res = RegEnumValueA(hKey, idx++, valueName, &valueNameSize,
                    nullptr, nullptr, nullptr, nullptr);
                if (res != ERROR_SUCCESS) break;
                if (strstr(valueName, "switchable") || strstr(valueName, "MediaSDK") ||
                    strstr(valueName, "MEDIASDK")) {
                    OutputDebugStringA("[VK] Broken implicit layer found in registry\n");
                    brokenLayerDetected = true;
                    break;
                }
            }
            RegCloseKey(hKey);
        }

        // Check for AMD switchable graphics DriverStore manifest
        if (!brokenLayerDetected) {
            const char* amdDriverDirs[] = {
                "C:\\Windows\\System32\\DriverStore\\FileRepository\\u0401134.inf_amd64_35bda0dee1499017\\B399690\\amd-vulkan64.json",
                "C:\\Windows\\System32\\DriverStore\\FileRepository\\amdwin-u0401134.inf_amd64_a9f23e8d30e00e27\\amd-vulkan64.json",
            };
            for (auto path : amdDriverDirs) {
                if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES) {
                    OutputDebugStringA("[VK] Broken AMD switchable graphics layer found in DriverStore\n");
                    brokenLayerDetected = true;
                    break;
                }
            }
        }

        if (brokenLayerDetected) {
            OutputDebugStringA("[VK] Broken implicit layers prevent safe Vulkan init. GDI fallback.\n");
            return false;
        }
    }

    // Load Vulkan DLL
    g_vkModule = LoadLibraryA("vulkan-1.dll");
    if (!g_vkModule) {
        OutputDebugStringA("[VK] Failed to load vulkan-1.dll\n");
        return false;
    }

    pfn_vkGetInstanceProcAddr =
        reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(g_vkModule, "vkGetInstanceProcAddr"));
    pfn_vkGetDeviceProcAddr =
        reinterpret_cast<PFN_vkGetDeviceProcAddr>(GetProcAddress(g_vkModule, "vkGetDeviceProcAddr"));

    if (!pfn_vkGetInstanceProcAddr) {
        OutputDebugStringA("[VK] vkGetInstanceProcAddr not found\n");
        return false;
    }

    // Load global-level function pointers
    pfn_vkCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        GetProcAddress(g_vkModule, "vkCreateInstance"));
    pfn_vkDestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        GetProcAddress(g_vkModule, "vkDestroyInstance"));
    pfn_vkCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        GetProcAddress(g_vkModule, "vkCreateDevice"));
    pfn_vkEnumeratePhysicalDevices = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
        GetProcAddress(g_vkModule, "vkEnumeratePhysicalDevices"));

    // Create instance
    if (!vk_inst::createInstance(ctx)) return false;
    vk_inst::loadInstanceFuncs(ctx);

    // Create debug messenger
    if (validationEnabled_ && pfn_vkCreateDebugUtilsMessengerEXT) {
        VkDebugUtilsMessengerCreateInfoEXT dbgInfo{};
        dbgInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        dbgInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        dbgInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        dbgInfo.pfnUserCallback = reinterpret_cast<PFN_vkDebugUtilsMessengerCallbackEXT>(vk_val::debugCallback);
        dbgInfo.pUserData = this;

        VkResult dbgResult = pfn_vkCreateDebugUtilsMessengerEXT(instance_, &dbgInfo, nullptr, &debugMessenger_);
        if (dbgResult == VK_SUCCESS) {
            OutputDebugStringA("[VK] Debug messenger created\n");
        } else {
            OutputDebugStringA("[VK] Failed to create debug messenger\n");
            debugMessenger_ = VK_NULL_HANDLE;
        }
    }

    // Create surface
    if (!vk_swap::createSurface(ctx, hwnd)) return false;

    // Pick physical device
    if (!vk_inst::pickPhysicalDevice(ctx)) return false;

    // Create logical device
    if (!vk_dev::createLogicalDevice(ctx)) return false;
    vk_dev::loadDeviceFuncs(ctx);
    pfn_vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    pfn_vkGetDeviceQueue(device_, presentFamily_, 0, &presentQueue_);

    // Create swapchain
    if (!vk_swap::createSwapchain(ctx, width, height)) return false;

    // Create sync objects
    if (!vk_sync::createSyncObjects(ctx)) return false;

    // Create command pool + buffers
    if (!vk_cmd::createCommandPool(ctx)) return false;
    if (!vk_cmd::allocateCommandBuffers(ctx)) return false;

    // Create descriptor pool
    if (!vk_desc::createDescriptorPool(ctx)) return false;

    // Create full-screen quad
    if (!vk_buf::createQuadBuffer(ctx)) return false;

    initialized_ = true;
    OutputDebugStringA("[VK] VulkanRenderer initialized successfully\n");
    return true;
}

// ============================================================================
// shutdown
// ============================================================================
void VulkanRenderer::shutdown() {
    if (!initialized_) return;

    VkCtx ctx{*this};

    if (device_) {
        pfn_vkDeviceWaitIdle(device_);
    }

    if (quadCreated_) {
        vk_buf::destroyBuffer(ctx, quadVbo_);
        quadCreated_ = false;
    }

    if (screenshotStagingCreated_) {
        vk_buf::destroyBuffer(ctx, screenshotStaging_);
        screenshotStagingCreated_ = false;
    }

    if (uploadCreated_) {
        vk_buf::destroyBuffer(ctx, uploadStaging_);
        vk_img::destroyImage(ctx, uploadImage_);
        uploadCreated_ = false;
    }

    if (descPool_) {
        pfn_vkDestroyDescriptorPool(device_, descPool_, nullptr);
        descPool_ = VK_NULL_HANDLE;
    }

    if (cmdPool_) {
        pfn_vkDestroyCommandPool(device_, cmdPool_, nullptr);
        cmdPool_ = VK_NULL_HANDLE;
    }

    if (syncCreated_) {
        for (uint32_t i = 0; i < kMaxFramesInFlight; i++) {
            pfn_vkDestroySemaphore(device_, imageAvailableSem_[i], nullptr);
            pfn_vkDestroySemaphore(device_, renderFinishedSem_[i], nullptr);
            pfn_vkDestroyFence(device_, inFlightFences_[i], nullptr);
        }
        syncCreated_ = false;
    }

    vk_swap::destroySwapchain(ctx);

    if (debugMessenger_) {
        pfn_vkDestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        debugMessenger_ = VK_NULL_HANDLE;
    }

    if (surface_) {
        pfn_vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }

    shutdownRequested_.store(true, std::memory_order_release);
    {
        constexpr int kThreadWaitMs = 30000;
        auto deadline = GetTickCount64() + kThreadWaitMs;
        while (pendingShaderThreads_.load(std::memory_order_acquire) > 0 && GetTickCount64() < deadline) {
            Sleep(50);
        }
        int remaining = pendingShaderThreads_.load(std::memory_order_acquire);
        if (remaining > 0) {
            char buf[128];
            sprintf_s(buf, "[VK] shutdown: %d shader threads still running after %d ms, proceeding anyway\n",
                      remaining, kThreadWaitMs);
            OutputDebugStringA(buf);
        }
    }

    if (device_) {
        pfn_vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }

    if (instance_) {
        pfn_vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }

    if (g_vkModule) {
        FreeLibrary(g_vkModule);
        g_vkModule = nullptr;
    }

    initialized_ = false;
    OutputDebugStringA("[VK] VulkanRenderer shut down\n");
}

// ============================================================================
// beginFrame
// ============================================================================
bool VulkanRenderer::beginFrame(uint32_t width, uint32_t height) {
    if (!initialized_ || frameActive_) return false;

    VkResult fenceResult = pfn_vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, 1000000000ULL);
    if (fenceResult == VK_TIMEOUT) {
        OutputDebugStringA("[VK] beginFrame: fence wait timeout, skipping frame\n");
        return false;
    }
    VkResult result = pfn_vkAcquireNextImageKHR(
        device_, swapchain_, UINT64_MAX,
        imageAvailableSem_[currentFrame_], VK_NULL_HANDLE,
        &currentImageIndex_);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        VkCtx ctx{*this};
        vk_swap::resize(ctx, width, height);
        return false;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (pfn_vkBeginCommandBuffer(cmdBuffers_[currentFrame_], &beginInfo) != VK_SUCCESS) {
        return false;
    }
    pfn_vkResetFences(device_, 1, &inFlightFences_[currentFrame_]);
    frameActive_ = true;
    return true;
}

// ============================================================================
// endFrame
// ============================================================================
void VulkanRenderer::endFrame() {
    if (!frameActive_) return;

    VkCtx ctx{*this};

    if (screenshot_.pending && screenshot_.width > 0 && screenshot_.height > 0 && !screenshotCopied_) {
        vk_ss::ensureScreenshotStaging(ctx, screenshot_.width, screenshot_.height);
        VkImage swapImg = currentSwapchainImage();
        if (swapImg) {
            vk_rec::cmdTransitionImage(ctx, swapImg,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            VkImageResource swapchainRes{};
            swapchainRes.image = swapImg;
            vk_rec::cmdCopyImageToBuffer(ctx, swapchainRes, screenshotStaging_, screenshot_.width, screenshot_.height);
            vk_rec::cmdTransitionImage(ctx, swapImg,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
            screenshotCopied_ = true;
        }
    }

    pfn_vkEndCommandBuffer(cmdBuffers_[currentFrame_]);
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

    VkResult submitResult = pfn_vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]);
    if (submitResult != VK_SUCCESS) {
        FILE* ef = nullptr;
        fopen_s(&ef, "build\\vk_pipeline_fail.log", "a");
        if (ef) { fprintf(ef, "vkQueueSubmit FAILED: VkResult=%d\n", (int)submitResult); fclose(ef); }
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSem_[currentFrame_];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImageIndex_;

    VkResult presentResult = pfn_vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR) {
        FILE* ef = nullptr;
        fopen_s(&ef, "build\\vk_pipeline_fail.log", "a");
        if (ef) { fprintf(ef, "vkQueuePresentKHR FAILED: VkResult=%d\n", (int)presentResult); fclose(ef); }
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

// ============================================================================
// waitForIdle
// ============================================================================
void VulkanRenderer::waitForIdle() {
    if (device_) {
        pfn_vkDeviceWaitIdle(device_);
    }
}

// ============================================================================
// Delegate methods — forward to decomposition modules
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

VkCommandBuffer VulkanRenderer::currentCommandBuffer() const {
    if (currentFrame_ < cmdBuffers_.size()) {
        return cmdBuffers_[currentFrame_];
    }
    return VK_NULL_HANDLE;
}

std::string VulkanRenderer::getVendor() const {
    VkCtx ctx{const_cast<VulkanRenderer&>(*this)};
    return vk_inst::getVendor(ctx);
}

std::string VulkanRenderer::getRenderer() const {
    VkCtx ctx{const_cast<VulkanRenderer&>(*this)};
    return vk_inst::getRenderer(ctx);
}

std::string VulkanRenderer::getVersion() const {
    VkCtx ctx{const_cast<VulkanRenderer&>(*this)};
    return vk_inst::getVersion(ctx);
}

void VulkanRenderer::requestScreenshot(uint32_t w, uint32_t h) {
    VkCtx ctx{*this};
    vk_ss::requestScreenshot(ctx, w, h);
}

std::vector<uint8_t> VulkanRenderer::takeScreenshot() {
    return std::move(screenshot_.pixels);
}

void VulkanRenderer::finalizeScreenshot() {
    VkCtx ctx{*this};
    vk_ss::finalizeScreenshot(ctx);
}

void VulkanRenderer::recordScreenshotCopy() {
    VkCtx ctx{*this};
    vk_ss::executeScreenshotCopy(ctx, screenshot_.width, screenshot_.height);
}

bool VulkanRenderer::uploadTexture(const void* bgraPixels, uint32_t w, uint32_t h) {
    VkCtx ctx{*this};
    return vk_upl::uploadTexture(ctx, bgraPixels, w, h);
}

bool VulkanRenderer::uploadTextureToImage(const void* bgraPixels, uint32_t w, uint32_t h, bool blitToSwapchain) {
    VkCtx ctx{*this};
    return vk_upl::uploadTextureToImage(ctx, bgraPixels, w, h, blitToSwapchain);
}

void VulkanRenderer::resize(uint32_t width, uint32_t height) {
    VkCtx ctx{*this};
    vk_swap::resize(ctx, width, height);
}

// Resource creation delegates
VkImageResource VulkanRenderer::createImage(uint32_t w, uint32_t h, VkFormat fmt,
                                             bool renderTarget, bool cpuReadable) {
    VkCtx ctx{*this};
    return vk_img::createImage(ctx, w, h, fmt, renderTarget, cpuReadable);
}

void VulkanRenderer::destroyImage(VkImageResource& img) {
    VkCtx ctx{*this};
    vk_img::destroyImage(ctx, img);
}

VkSamplerResource VulkanRenderer::createSampler(VkFilter minFilter, VkFilter magFilter,
                                                  VkSamplerAddressMode wrapS, VkSamplerAddressMode wrapT,
                                                  float borderColorR, float borderColorG,
                                                  float borderColorB, float borderColorA) {
    VkCtx ctx{*this};
    return vk_smp::createSampler(ctx, minFilter, magFilter, wrapS, wrapT,
                                  borderColorR, borderColorG, borderColorB, borderColorA);
}

void VulkanRenderer::destroySampler(VkSamplerResource& s) {
    VkCtx ctx{*this};
    vk_smp::destroySampler(ctx, s);
}

VkBufferResource VulkanRenderer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                                               VkMemoryPropertyFlags props) {
    VkCtx ctx{*this};
    return vk_buf::createBuffer(ctx, size, usage, props);
}

void VulkanRenderer::destroyBuffer(VkBufferResource& buf) {
    VkCtx ctx{*this};
    vk_buf::destroyBuffer(ctx, buf);
}

void VulkanRenderer::uploadBuffer(VkBufferResource& dst, const void* data, VkDeviceSize size) {
    VkCtx ctx{*this};
    vk_buf::uploadBuffer(ctx, dst, data, size);
}

VkDescriptorSetLayout VulkanRenderer::createDescriptorSetLayout(
    std::span<const VkDescriptorSetLayoutBinding> bindings) {
    VkCtx ctx{*this};
    return vk_desc::createDescriptorSetLayout(ctx, bindings);
}

void VulkanRenderer::destroyDescriptorSetLayout(VkDescriptorSetLayout layout) {
    VkCtx ctx{*this};
    vk_desc::destroyDescriptorSetLayout(ctx, layout);
}

VkDescriptorSet VulkanRenderer::allocateDescriptorSet(VkDescriptorSetLayout layout) {
    VkCtx ctx{*this};
    return vk_desc::allocateDescriptorSet(ctx, layout);
}

void VulkanRenderer::updateDescriptorSetTexture(VkDescriptorSet set, uint32_t binding,
                                                  VkImageView view, VkSampler sampler,
                                                  VkImageLayout layout) {
    VkCtx ctx{*this};
    vk_desc::updateDescriptorSetTexture(ctx, set, binding, view, sampler, layout);
}

void VulkanRenderer::updateDescriptorSetUniform(VkDescriptorSet set, uint32_t binding,
                                                  VkBuffer buffer, VkDeviceSize range) {
    VkCtx ctx{*this};
    vk_desc::updateDescriptorSetUniform(ctx, set, binding, buffer, range);
}

VkPipelineLayout VulkanRenderer::createPipelineLayout(
    std::span<const VkDescriptorSetLayout> descLayouts,
    std::span<const VkPushConstantRange> pushRanges) {
    VkCtx ctx{*this};
    return vk_pipe::createPipelineLayout(ctx, descLayouts, pushRanges);
}

void VulkanRenderer::destroyPipelineLayout(VkPipelineLayout layout) {
    VkCtx ctx{*this};
    vk_pipe::destroyPipelineLayout(ctx, layout);
}

VkPipeline VulkanRenderer::createGraphicsPipeline(
    std::span<const uint32_t> vertSpirv,
    std::span<const uint32_t> fragSpirv,
    VkPipelineLayout layout,
    VkFormat colorFormat,
    VkFormat depthFormat,
    bool hasBlend,
    VkVertexInputBindingDescription vertBinding,
    std::span<const VkVertexInputAttributeDescription> vertAttrs) {
    VkCtx ctx{*this};
    return vk_pipe::createGraphicsPipeline(ctx, vertSpirv, fragSpirv, layout,
                                            colorFormat, depthFormat, hasBlend,
                                            vertBinding, vertAttrs);
}

void VulkanRenderer::destroyPipeline(VkPipeline pipeline) {
    VkCtx ctx{*this};
    vk_pipe::destroyPipeline(ctx, pipeline);
}

VkShaderModule VulkanRenderer::createShaderModule(std::span<const uint32_t> spirv) {
    VkCtx ctx{*this};
    return vk_pipe::createShaderModule(ctx, spirv);
}

void VulkanRenderer::destroyShaderModule(VkShaderModule mod) {
    VkCtx ctx{*this};
    vk_pipe::destroyShaderModule(ctx, mod);
}

void VulkanRenderer::cmdBeginRendering(VkImageView colorTarget, uint32_t w, uint32_t h,
                                        VkClearValue clear) {
    VkCtx ctx{*this};
    vk_rec::cmdBeginRendering(ctx, colorTarget, w, h, clear);
}

void VulkanRenderer::cmdEndRendering() {
    VkCtx ctx{*this};
    vk_rec::cmdEndRendering(ctx);
}

void VulkanRenderer::cmdBindPipeline(VkPipeline pipeline, VkPipelineLayout layout) {
    VkCtx ctx{*this};
    vk_rec::cmdBindPipeline(ctx, pipeline, layout);
}

void VulkanRenderer::cmdBindDescriptorSet(VkPipelineLayout layout, VkDescriptorSet set, uint32_t setIdx) {
    VkCtx ctx{*this};
    vk_rec::cmdBindDescriptorSet(ctx, layout, set, setIdx);
}

void VulkanRenderer::cmdBindVertexBuffer(VkBufferResource& buf) {
    VkCtx ctx{*this};
    vk_rec::cmdBindVertexBuffer(ctx, buf);
}

void VulkanRenderer::cmdSetViewport(uint32_t w, uint32_t h) {
    VkCtx ctx{*this};
    vk_rec::cmdSetViewport(ctx, w, h);
}

void VulkanRenderer::cmdSetScissor(uint32_t w, uint32_t h) {
    VkCtx ctx{*this};
    vk_rec::cmdSetScissor(ctx, w, h);
}

void VulkanRenderer::cmdPushConstants(VkPipelineLayout layout, VkShaderStageFlags stage,
                                       uint32_t offset, uint32_t size, const void* data) {
    VkCtx ctx{*this};
    vk_rec::cmdPushConstants(ctx, layout, stage, offset, size, data);
}

void VulkanRenderer::cmdDraw(uint32_t vertexCount, uint32_t firstVertex) {
    VkCtx ctx{*this};
    vk_rec::cmdDraw(ctx, vertexCount, firstVertex);
}

void VulkanRenderer::cmdBlitImage(VkImageResource& src, VkImageResource& dst,
                                   uint32_t srcW, uint32_t srcH, uint32_t dstW, uint32_t dstH) {
    VkCtx ctx{*this};
    vk_rec::cmdBlitImage(ctx, src, dst, srcW, srcH, dstW, dstH);
}

void VulkanRenderer::cmdCopyImageToBuffer(VkImageResource& src, VkBufferResource& dst,
                                           uint32_t w, uint32_t h) {
    VkCtx ctx{*this};
    vk_rec::cmdCopyImageToBuffer(ctx, src, dst, w, h);
}

void VulkanRenderer::cmdCopyBufferToImage(VkBufferResource& src, VkImageResource& dst,
                                           uint32_t w, uint32_t h) {
    VkCtx ctx{*this};
    vk_rec::cmdCopyBufferToImage(ctx, src, dst, w, h);
}

void VulkanRenderer::cmdTransitionLayout(VkImageResource& img, VkImageLayout newLayout) {
    VkCtx ctx{*this};
    vk_rec::cmdTransitionLayout(ctx, img, newLayout);
}

void VulkanRenderer::cmdTransitionImage(VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout,
                                         VkImageSubresourceRange range) {
    VkCtx ctx{*this};
    vk_rec::cmdTransitionImage(ctx, img, oldLayout, newLayout, range);
}

void VulkanRenderer::drawFullScreenQuad() {
    VkCtx ctx{*this};
    vk_rec::drawFullScreenQuad(ctx);
}

bool VulkanRenderer::loadPreset(uint32_t width, uint32_t height,
    const std::vector<std::vector<uint32_t>>& vertSpvs,
    const std::vector<std::vector<uint32_t>>& fragSpvs,
    const std::vector<uint32_t>& samplerCounts,
    const std::vector<uint32_t>& pushSizes,
    const std::vector<VkFormat>& rtFormats,
    const std::vector<std::vector<float>>& pushDefaultsList,
    const std::vector<uint32_t>& uboSizes) {
    VkCtx ctx{*this};
    std::vector<uint32_t> rtFormatsU32(rtFormats.begin(), rtFormats.end());
    return vk_preset::loadPreset(ctx, width, height, vertSpvs, fragSpvs,
                                   samplerCounts, pushSizes, rtFormatsU32,
                                   pushDefaultsList, uboSizes);
}

bool VulkanRenderer::loadPresetReflection(uint32_t width, uint32_t height,
    const std::vector<std::vector<uint32_t>>& vertSpvs,
    const std::vector<std::vector<uint32_t>>& fragSpvs,
    const std::vector<uint32_t>& pushSizes,
    const std::vector<VkFormat>& rtFormats,
    const std::vector<std::vector<float>>& pushDefaultsList,
    const std::vector<uint32_t>& uboSizes,
    const std::vector<std::vector<SamplerBindingInfo>>& perPassSamplerBindings) {
    VkCtx ctx{*this};
    std::vector<uint32_t> rtFormatsU32(rtFormats.begin(), rtFormats.end());
    std::vector<std::vector<PresetSamplerBinding>> samplerBindings;
    samplerBindings.reserve(perPassSamplerBindings.size());
    for (auto& passBindings : perPassSamplerBindings) {
        std::vector<PresetSamplerBinding> converted;
        converted.reserve(passBindings.size());
        for (auto& b : passBindings) {
            converted.push_back({b.binding, b.samplerName});
        }
        samplerBindings.push_back(std::move(converted));
    }
    return vk_preset::loadPresetReflection(ctx, width, height, vertSpvs, fragSpvs,
                                             pushSizes, rtFormatsU32, pushDefaultsList,
                                             uboSizes, samplerBindings);
}

void VulkanRenderer::destroyPreset() {
    VkCtx ctx{*this};
    vk_preset::destroyPreset(ctx);
}

bool VulkanRenderer::renderPreset(uint32_t width, uint32_t height) {
    VkCtx ctx{*this};
    return vk_preset::renderPreset(ctx, width, height);
}

bool VulkanRenderer::blitAppContentOverPreset(uint32_t width, uint32_t height) {
    VkCtx ctx{*this};
    return vk_preset::blitAppContentOverPreset(ctx, width, height);
}

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkCtx ctx{*this};
    return vk_mem::findMemoryType(ctx, typeFilter, props);
}

bool VulkanRenderer::allocateImageMemory(VkImage image, VkMemoryPropertyFlags props, VkDeviceMemory* outMem) {
    VkCtx ctx{*this};
    return vk_mem::allocateImageMemory(ctx, image, props, outMem);
}

bool VulkanRenderer::transitionImageLayoutImmediate(VkImage img, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCtx ctx{*this};
    return vk_cmd::transitionImageLayoutImmediate(ctx, img, oldLayout, newLayout);
}
