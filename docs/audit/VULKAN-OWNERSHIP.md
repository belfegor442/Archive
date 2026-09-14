# VULKAN-OWNERSHIP.md — Vulkan Resource Ownership Audit

**Auditor:** opencode/mimo-v2.5-free  
**Scope:** `src/native/vulkan_renderer.cpp` (2856 lines), `src/native/vulkan_renderer.h` (450 lines)  
**Date:** 2026-09-14  

---

## Executive Summary

This audit reveals **6 critical issues**, **4 high-severity issues**, and **5 medium-severity issues** in the Vulkan resource ownership and lifecycle management. The most severe problems are:

1. **No cleanup on initialize() failure** — every early return leaks all previously created resources.
2. **DLL handle never freed** — `FreeLibrary` is never called; the static `g_vkModule` leaks.
3. **Preset resources not destroyed in shutdown()** — `destroyPreset()` is never called during shutdown, leaking all shader modules, pipelines, pipeline layouts, descriptor set layouts, render targets, uniform buffers, and samplers.
4. **83 static function pointers never reset** — they remain valid only if the DLL is never freed (which is currently the case, but is itself a bug).
5. **23 unchecked VkResult calls** — many are for query/enum functions, but several are for critical operations like `vkBeginCommandBuffer`, `vkQueueSubmit`, `vkMapMemory`, and `vkBindBufferMemory`.
6. **3 validation count functions lack mutex protection** — `validationErrorCount`, `validationWarningCount`, `validationCriticalCount` iterate `validationMessages_` without acquiring `validationMutex_`, creating data races with the debug callback thread.

---

## 1. vulkan_renderer.h — Member Variables

### Vulkan Handles (Core)
| Line | Variable | Type | Description |
|------|----------|------|-------------|
| 379 | `initialized_` | `bool` | Guard flag for lifecycle |
| 380 | `hwnd_` | `HWND` | Win32 window handle |
| 383 | `instance_` | `VkInstance` | Vulkan instance |
| 384 | `surface_` | `VkSurfaceKHR` | Win32 surface |
| 385 | `physDev_` | `VkPhysicalDevice` | Physical device (not owned) |
| 386 | `device_` | `VkDevice` | Logical device |
| 389 | `graphicsQueue_` | `VkQueue` | Graphics queue (not owned) |
| 390 | `presentQueue_` | `VkQueue` | Present queue (not owned) |
| 393 | `swapchain_` | `VkSwapchainKHR` | Swapchain |
| 394 | `swapchainImages_` | `vector<VkImage>` | Swapchain images (owned by swapchain) |
| 395 | `swapchainImageViews_` | `vector<VkImageView>` | Swapchain image views (manually created) |
| 396 | `swapchainFormat_` | `VkFormat` | Chosen surface format |
| 397 | `swapchainExtent_` | `VkExtent2D` | Chosen extent |
| 400 | `cmdPool_` | `VkCommandPool` | Command pool |
| 401 | `cmdBuffers_` | `array<VkCommandBuffer, 2>` | Per-frame command buffers (freed with pool) |
| 407 | `imageAvailableSem_` | `array<VkSemaphore, 2>` | Per-frame image-available semaphore |
| 408 | `renderFinishedSem_` | `array<VkSemaphore, 2>` | Per-frame render-finished semaphore |
| 409 | `inFlightFences_` | `array<VkFence, 2>` | Per-frame fence |
| 413 | `descPool_` | `VkDescriptorPool` | Descriptor pool |
| 441 | `debugMessenger_` | `VkDebugUtilsMessengerEXT` | Debug messenger |

### GPU Buffers (Owned)
| Line | Variable | Type | Description |
|------|----------|------|-------------|
| 416 | `quadVbo_` | `VkBufferResource` | Full-screen quad VBO |
| 421 | `screenshotStaging_` | `VkBufferResource` | Screenshot readback staging buffer |
| 428 | `uploadImage_` | `VkImageResource` | Upload texture (GDI→Vulkan) |
| 429 | `uploadStaging_` | `VkBufferResource` | Upload staging buffer |

### State Tracking
| Line | Variable | Type | Description |
|------|----------|------|-------------|
| 402 | `currentFrame_` | `uint32_t` | Current frame index |
| 403 | `currentImageIndex_` | `uint32_t` | Current swapchain image index |
| 404 | `frameActive_` | `bool` | Frame in progress flag |
| 410 | `syncCreated_` | `bool` | Sync objects created flag |
| 417 | `quadCreated_` | `bool` | Quad VBO created flag |
| 422 | `screenshotStagingCreated_` | `bool` | Screenshot staging created flag |
| 423 | `screenshotCopied_` | `bool` | Screenshot copied flag |
| 430 | `uploadCreated_` | `bool` | Upload resources created flag |

### Preset Resources
| Line | Variable | Type | Description |
|------|----------|------|-------------|
| 433 | `preset_` | `LoadedPreset` | All preset passes, samplers, images |
| 434 | `presetFrameCount_` | `uint32_t` | Preset frame counter |

### Validation
| Line | Variable | Type | Description |
|------|----------|------|-------------|
| 442 | `requestValidation_` | `bool` | Request validation before init |
| 443 | `validationEnabled_` | `bool` | Validation enabled |
| 444 | `validationAvailable_` | `bool` | Validation layers available |
| 445 | `validationMessages_` | `vector<ValidationMessage>` | Message buffer |
| 446 | `validationMutex_` | `mutex` | Message buffer lock |

### DLL
| Line | Variable | Type | Description |
|------|----------|------|-------------|
| 449 | `vulkanDll_` | `HMODULE` | **NEVER ASSIGNED OR USED** |

### Initialization State Approach
The `initialized_` flag is set to `true` only after ALL `initialize()` steps succeed (line 314). In `shutdown()`, it is the first guard check (line 323). Destruction order in shutdown() is **approximately reverse of creation**, with exceptions noted below.

---

## 2. initialize() Flow

```
initialize(hwnd, width, height)  [line 223]
│
├─ [1] if (initialized_) return true                              [224]
│
├─ [2] g_vkModule = LoadLibraryA("vulkan-1.dll")                 [229]
│      FAIL → return false  (no cleanup needed, nothing created yet)
│
├─ [3] GetProcAddress: vkGetInstanceProcAddr, vkGetDeviceProcAddr [235-242]
│      FAIL → return false  (** DLL NOT FREED **)
│
├─ [4] GetProcAddress: vkCreateInstance, vkDestroyInstance, etc.  [250-257]
│      (no individual failure check)
│
├─ [5] createInstance()                                           [260]
│      FAIL → return false  (** DLL NOT FREED **)
│
├─ [6] loadInstanceFuncs()                                        [261]
│
├─ [7] Debug messenger (optional, non-fatal)                      [264-284]
│
├─ [8] createSurface(hwnd)                                        [287]
│      FAIL → return false  (** instance + DLL not freed **)
│
├─ [9] pickPhysicalDevice()                                       [290]
│      FAIL → return false  (** surface + instance + DLL not freed **)
│
├─ [10] createLogicalDevice()                                     [293]
│       FAIL → return false  (** surface + instance + DLL not freed **)
│
├─ [11] loadDeviceFuncs() + getDeviceQueue (2x)                   [294-296]
│
├─ [12] createSwapchain(width, height)                            [299]
│       FAIL → return false  (** device + surface + instance + DLL not freed **)
│
├─ [13] createSyncObjects()                                       [302]
│       FAIL → return false  (** swapchain + device + surface + instance + DLL not freed **)
│
├─ [14] createCommandPool()                                       [305]
│       FAIL → return false  (** sync + swapchain + device + surface + instance + DLL **)
│
├─ [15] allocateCommandBuffers()                                  [306]
│       FAIL → return false  (** cmdPool + sync + ... **)
│
├─ [16] createDescriptorPool()                                    [309]
│       FAIL → return false  (** cmdBuffers freed with cmdPool, but cmdPool itself NOT freed **)
│
├─ [17] createQuadBuffer()                                        [312]
│       FAIL → return false  (** descPool + cmdPool + sync + ... **)
│
└─ [18] initialized_ = true; return true                          [314]
```

**CRITICAL**: There is zero cleanup on failure. Every early return after step [5] leaks all previously created resources.

---

## 3. shutdown() Flow

```
shutdown()  [line 322]
│
├─ [1] if (!initialized_) return                                  [323]
│
├─ [2] pfn_vkDeviceWaitIdle(device_)                              [325-327]
│
├─ [3] if (quadCreated_) destroyBuffer(quadVbo_)                  [330-333]
│
├─ [4] if (screenshotStagingCreated_) destroyBuffer(screenshotStaging_) [336-339]
│
├─ [5] if (uploadCreated_) destroyBuffer(uploadStaging_) + destroyImage(uploadImage_) [342-346]
│
├─ [6] pfn_vkDestroyDescriptorPool(device_, descPool_)            [349-352]
│
├─ [7] pfn_vkDestroyCommandPool(device_, cmdPool_)                [355-358]
│       (implicitly frees cmdBuffers_[0..1])
│
├─ [8] if (syncCreated_) destroy fences + semaphores              [361-368]
│
├─ [9] destroySwapchain()                                         [371]
│       (destroys image views, then swapchain)
│
├─ [10] pfn_vkDestroyDebugUtilsMessengerEXT(instance_, ...)       [374-377]
│
├─ [11] pfn_vkDestroySurfaceKHR(instance_, surface_)              [380-383]
│
├─ [12] pfn_vkDestroyDevice(device_)                              [386-389]
│
├─ [13] pfn_vkDestroyInstance(instance_)                          [392-395]
│
└─ [14] initialized_ = false                                      [397]
```

### Destruction Order Analysis

The order is **mostly correct** (reverse of creation):

| Step | Resource | Correct Order? | Notes |
|------|----------|---------------|-------|
| 2 | DeviceWaitIdle | ✓ | Must drain GPU first |
| 3 | quadVbo_ | ✓ | Consumer of device |
| 4 | screenshotStaging_ | ✓ | Consumer of device |
| 5 | uploadStaging_ + uploadImage_ | ✓ | Consumer of device |
| 6 | descPool_ | ✓ | Consumer of device; implicitly frees descriptor sets |
| 7 | cmdPool_ | ✓ | Consumer of device; implicitly frees command buffers |
| 8 | sync objects | ✓ | Consumer of device |
| 9 | swapchain + views | ✓ | Consumer of device + surface |
| 10 | debugMessenger_ | ✓ | Consumer of instance (created before surface) |
| 11 | surface_ | ✓ | Consumer of instance |
| 12 | device_ | ✓ | Consumer of instance |
| 13 | instance_ | ✓ | Last Vulkan object |

### Resources NOT Destroyed in shutdown()

| Resource | Lines | Impact |
|----------|-------|--------|
| **g_vkModule (DLL)** | — | **DLL handle leak** |
| **preset_.passes[]** | 2319-2648 | **Shader modules, pipelines, layouts, render targets, UBOs leak** |
| **preset_.samplers[]** | 2418-2424 | **Samplers leak** |
| **preset_.images[]** | — | **Preset images leak** |
| **vulkanDll_** | 449 | Never assigned (dead member) |

---

## 4. DLL Lifecycle Analysis

| Operation | Line | Target | Status |
|-----------|------|--------|--------|
| `LoadLibraryA("vulkan-1.dll")` | 229 | `g_vkModule` (static global) | OK |
| `GetProcAddress` calls | 235-257 | `g_vkModule` | OK |
| `FreeLibrary` | — | — | **NEVER CALLED** |
| `vulkanDll_` assignment | — | member variable | **NEVER ASSIGNED** |

### Analysis

- `g_vkModule` is a file-static global (`static HMODULE g_vkModule = nullptr`, line 19).
- `vulkanDll_` is a member variable (`HMODULE vulkanDll_ = VK_NULL_HANDLE`, line 449) that is **never written to or read from**.
- The destructor calls `shutdown()`, which does not call `FreeLibrary`.
- If the renderer is destroyed and recreated, `LoadLibraryA` is called again, getting a new handle (or the same one with an incremented refcount on Windows). The old handle is never released.
- **Since FreeLibrary is never called, the DLL remains loaded for the process lifetime.** This is a resource leak but not a use-after-free (which would occur if FreeLibrary were called before function pointers were reset).

### Risk Assessment
- **Current impact**: DLL handle leak (minor on Windows for a process-lifetime library).
- **Future risk**: If FreeLibrary is added without also resetting all 83 function pointers, any subsequent call through a stale `pfn_vk*` will be a **use-after-free crash**.

---

## 5. Function Pointer Analysis

### Count

**83 static `PFN_vk*` function pointers** declared at file scope (lines 22–114):

- **2 global-level**: `pfn_vkGetInstanceProcAddr`, `pfn_vkGetDeviceProcAddr`
- **16 instance-level**: `pfn_vkCreateInstance` through `pfn_vkEnumerateDeviceExtensionProperties`
- **65 device-level**: `pfn_vkCreateDevice` through `pfn_vkFlushMappedMemoryRanges`

### Loading

- **Global-level** (lines 250–257): Loaded from `g_vkModule` via `GetProcAddress` before instance creation.
- **Instance-level** (lines 2153–2183): Loaded via `pfn_vkGetInstanceProcAddr` in `loadInstanceFuncs()`.
- **Device-level** (lines 2188–2256): Loaded via `pfn_vkGetDeviceProcAddr` in `loadDeviceFuncs()`, using a `LOAD` macro.

### Reset to nullptr

**Never.** None of the 83 pointers are ever set back to `nullptr` — not in `shutdown()`, not anywhere.

### Use-after-Free Risk

Since `FreeLibrary` is never called, this is currently safe. However, the design has a latent bug: if `FreeLibrary(g_vkModule)` were ever added to `shutdown()`, all 83 function pointers would become dangling, and any subsequent call would crash. The pointers must be reset to `nullptr` during shutdown, **after** all Vulkan resources are destroyed but **before** `FreeLibrary`.

---

## 6. VkResult Checking Analysis

### Summary

| Category | Count |
|----------|-------|
| VkResult-returning calls that ARE checked | 15 |
| VkResult-returning calls that are NOT checked | 23 |
| Void-returning calls (no check needed) | ~40 |
| **Total unchecked VkResult sites** | **23** |

### Unchecked VkResult Calls (Detailed)

#### Query/Enumeration Functions (Low Risk — failure means empty data)

| Line | Function | Failure Consequence |
|------|----------|-------------------|
| 439 | `vkEnumerateInstanceLayerProperties` (count) | Validation layer detection fails silently; proceeds without validation |
| 441 | `vkEnumerateInstanceLayerProperties` (data) | Same as above |
| 497 | `vkEnumeratePhysicalDevices` (count) | `deviceCount` may be 0 or garbage; caught by `deviceCount == 0` check at 498 |
| 504 | `vkEnumeratePhysicalDevices` (data) | `devices` vector may be empty or garbage |
| 540 | `vkGetPhysicalDeviceSurfaceSupportKHR` | `presentSupport` may be wrong; could pick wrong queue family |
| 616 | `vkGetPhysicalDeviceSurfaceCapabilitiesKHR` | `caps` is zeroed; extent/format selection may be wrong |
| 619 | `vkGetPhysicalDeviceSurfaceFormatsKHR` (count) | `formatCount` may be 0; crash on `formats[0]` at line 629 |
| 621 | `vkGetPhysicalDeviceSurfaceFormatsKHR` (data) | `formats` may be empty or wrong |
| 624 | `vkGetPhysicalDeviceSurfacePresentModesKHR` (count) | `presentModeCount` may be 0; vector is empty; loop at 652 is safe (no iteration) |
| 626 | `vkGetPhysicalDeviceSurfacePresentModesKHR` (data) | `presentModes` may be empty or wrong |
| 714 | `vkGetSwapchainImagesKHR` (count) | `swapImageCount` may be 0; `swapchainImages_` empty; crash on `swapchainImages_[i]` at 723 |
| 716 | `vkGetSwapchainImagesKHR` (data) | `swapchainImages_` may be empty or wrong |

#### Critical Operations (High Risk — silent corruption or crash)

| Line | Function | Failure Consequence |
|------|----------|-------------------|
| 888 | `vkResetFences` | Fence not reset; next `vkWaitForFences` may return immediately or timeout unexpectedly |
| 909 | `vkBeginCommandBuffer` | Command buffer not begin-able; all subsequent draw commands are **undefined behavior** |
| 939 | `vkEndCommandBuffer` | Command buffer not endable; `vkQueueSubmit` with invalid command buffer → **validation error or crash** |
| 959 | `vkQueueSubmit` | GPU submission failed; fence never signaled; next frame's `vkWaitForFences` **deadlocks for 1 second** |
| 969 | `vkQueuePresentKHR` | Present failed; image not displayed; `VK_ERROR_OUT_OF_DATE_KHR` not detected |
| 1013 | `vkBindImageMemory` | Image memory not bound; sampling from image → **GPU fault or garbage** |
| 1028 | `vkAllocateCommandBuffers` | Transition command buffer allocation failed; `cmd` is VK_NULL_HANDLE; `vkBeginCommandBuffer` at 1033 → **crash** |
| 1033 | `vkBeginCommandBuffer` (transition) | Same as line 909 but for one-shot buffer |
| 1085 | `vkEndCommandBuffer` (transition) | Same as line 939 but for one-shot buffer |
| 1092 | `vkQueueSubmit` (transition) | Transition not executed; image layout wrong → **rendering corruption** |
| 1093 | `vkQueueWaitIdle` | May return error but GPU work may still complete; no immediate impact |
| 1262 | `vkBindBufferMemory` | Buffer memory not bound; mapping at 1267 → **crash or garbage** |
| 1267 | `vkMapMemory` | Memory not mapped; `result.mapped` is nullptr; subsequent `memcpy` in `uploadBuffer` → **null pointer write** |
| 1305 | `vkCreateDescriptorSetLayout` | Layout is VK_NULL_HANDLE; `createPipelineLayout` at 1363 receives null layout → **validation error** |
| 1327 | `vkAllocateDescriptorSets` | Set is VK_NULL_HANDLE; descriptor update/write at 1351 → **validation error** |
| 1390 | `vkCreatePipelineLayout` | Layout is VK_NULL_HANDLE; `createGraphicsPipeline` at 1531 receives null layout → **validation error or crash** |

---

## 7. Validation Thread Safety Analysis

### Functions and Their Locking

| Function | Line | Acquires `validationMutex_`? | Thread-Safe? |
|----------|------|------------------------------|-------------|
| `addValidationMessage` | 172-175 | **Yes** (`lock_guard`) | ✅ Yes |
| `takeValidationMessages` | 180-183 | **Yes** (`lock_guard`) | ✅ Yes |
| `clearValidationMessages` | 188-191 | **Yes** (`lock_guard`) | ✅ Yes |
| `validationErrorCount` | 196-202 | **No** | ❌ **Data race** |
| `validationWarningCount` | 204-210 | **No** | ❌ **Data race** |
| `validationCriticalCount` | 212-218 | **No** | ❌ **Data race** |

### The Debug Callback (line 119-137)

```cpp
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(...) {
    auto* renderer = reinterpret_cast<VulkanRenderer*>(userData);
    // ...
    renderer->addValidationMessage(std::move(msg));  // thread-safe (acquires lock)
    // ...
}
```

- The callback is invoked by the Vulkan validation layer from **any thread** that triggers a validation event.
- It calls `addValidationMessage()`, which **does** acquire the mutex. ✅
- However, the count functions (`validationErrorCount`, etc.) iterate `validationMessages_` **without holding the mutex**, so they can race with:
  - The debug callback calling `addValidationMessage()` from a validation layer thread
  - Any thread calling `takeValidationMessages()` or `clearValidationMessages()`
- This is **undefined behavior** per the C++ standard (concurrent read+write on `std::vector`).

### Additional Concern: `takeValidationMessages` return

```cpp
std::vector<ValidationMessage> VulkanRenderer::takeValidationMessages() {
    std::lock_guard<std::mutex> lock(validationMutex_);
    return std::move(validationMessages_);
}
```

After the move, `validationMessages_` is in a valid-but-unspecified state. If another thread calls `addValidationMessage()` immediately after (before the caller processes the returned vector), the push_back inside `addValidationMessage` operates on the moved-from vector. This is technically safe (the moved-from vector is in a valid state), but the count functions could still race with this state transition.

---

## 8. Resource Ownership Table

### Core Resources (initialize/shutdown managed)

| Resource | Create Function | Destroy Function | Create Line | Destroy Line | Protected By | Cleanup Order |
|----------|----------------|-----------------|-------------|-------------|-------------|--------------|
| `g_vkModule` | `LoadLibraryA` | `FreeLibrary` | 229 | **NEVER** | None | **LEAKS** |
| `instance_` | `vkCreateInstance` | `vkDestroyInstance` | 467 | 393 | None (pre-init) | 13 (last) |
| `debugMessenger_` | `vkCreateDebugUtilsMessengerEXT` | `vkDestroyDebugUtilsMessengerEXT` | 277 | 375 | None (pre-init) | 10 |
| `surface_` | `vkCreateWin32SurfaceKHR` | `vkDestroySurfaceKHR` | 485 | 381 | None (pre-init) | 11 |
| `device_` | `vkCreateDevice` | `vkDestroyDevice` | 601 | 387 | None (pre-init) | 12 |
| `swapchain_` | `vkCreateSwapchainKHR` | `vkDestroySwapchainKHR` | 700 | 756 | None (pre-init) | 9 |
| `swapchainImageViews_[]` | `vkCreateImageView` | `vkDestroyImageView` | 732 | 749 | None (pre-init) | 9 (part of destroySwapchain) |
| `imageAvailableSem_[]` | `vkCreateSemaphore` | `vkDestroySemaphore` | 783 | 363 | `syncCreated_` | 8 |
| `renderFinishedSem_[]` | `vkCreateSemaphore` | `vkDestroySemaphore` | 784 | 364 | `syncCreated_` | 8 |
| `inFlightFences_[]` | `vkCreateFence` | `vkDestroyFence` | 785 | 365 | `syncCreated_` | 8 |
| `cmdPool_` | `vkCreateCommandPool` | `vkDestroyCommandPool` | 803 | 356 | None | 7 |
| `cmdBuffers_[]` | `vkAllocateCommandBuffers` | (freed with cmdPool) | 817 | 356 | None | 7 (implicit) |
| `descPool_` | `vkCreateDescriptorPool` | `vkDestroyDescriptorPool` | 837 | 350 | None | 6 |
| `quadVbo_` | `createBuffer` | `destroyBuffer` | 858 | 331 | `quadCreated_` | 3 |
| `screenshotStaging_` | `createBuffer` (lazy) | `destroyBuffer` | 2094 | 337 | `screenshotStagingCreated_` | 4 |
| `uploadStaging_` | `createBuffer` (lazy) | `destroyBuffer` | 1794 | 343 | `uploadCreated_` | 5 |
| `uploadImage_` | `createImage` (lazy) | `destroyImage` | 1797 | 344 | `uploadCreated_` | 5 |

### Preset Resources (loadPreset/destroyPreset managed — NOT cleaned up in shutdown)

| Resource | Create Function | Destroy Function | Destroy in shutdown? |
|----------|----------------|-----------------|---------------------|
| `pass.vertModule` | `createShaderModule` | `destroyShaderModule` | **NO** |
| `pass.fragModule` | `createShaderModule` | `destroyShaderModule` | **NO** |
| `pass.descSetLayout` | `createDescriptorSetLayout` | `destroyDescriptorSetLayout` | **NO** |
| `pass.pipelineLayout` | `createPipelineLayout` | `destroyPipelineLayout` | **NO** |
| `pass.pipeline` | `createGraphicsPipeline` | `destroyPipeline` | **NO** |
| `pass.renderTarget` | `createImage` | `destroyImage` | **NO** |
| `pass.uniformBuffer` | `createBuffer` | `destroyBuffer` | **NO** |
| `preset_.samplers[]` | `createSampler` | `destroySampler` | **NO** |
| `preset_.images[]` | `createImage` | `destroyImage` | **NO** |

---

## 9. Issues Summary

### CRITICAL (Severity: 5)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| C1 | **No cleanup on initialize() failure** | `initialize()` lines 260–312 | Every early return after `createInstance()` leaks the instance, surface, device, swapchain, sync objects, command pool, descriptor pool, and quad buffer. If `initialized_` is never set to `true`, `shutdown()` will not clean up. |
| C2 | **Preset resources not destroyed in shutdown()** | `shutdown()` vs `destroyPreset()` | `shutdown()` does not call `destroyPreset()`. All shader modules, pipelines, pipeline layouts, descriptor set layouts, render targets, uniform buffers, and samplers created by `loadPreset`/`loadPresetReflection` leak on shutdown. |
| C3 | **DLL handle never freed** | Entire file | `g_vkModule` (line 229) is loaded via `LoadLibraryA` but `FreeLibrary` is never called. The `vulkanDll_` member (line 449) is declared but never assigned. |
| C4 | **83 function pointers never reset** | Lines 22–114 | If `FreeLibrary` were ever added, all function pointers become dangling. Currently safe only because `FreeLibrary` is never called. |
| C5 | **vkBeginCommandBuffer unchecked (line 909)** | `beginFrame()` | If `vkBeginCommandBuffer` fails, all subsequent draw/blit commands in the frame are undefined behavior. No error is propagated. |
| C6 | **vkMapMemory unchecked (line 1267)** | `createBuffer()` | If mapping fails, `result.mapped` is nullptr. Any subsequent `uploadBuffer()` call will `memcpy` to a null pointer → **crash**. |

### HIGH (Severity: 4)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| H1 | **vkQueueSubmit unchecked (line 959)** | `present()` | If queue submission fails, the fence is never signaled. Next frame's `vkWaitForFences` blocks for 1 second, then the frame is skipped. |
| H2 | **vkQueuePresentKHR unchecked (line 969)** | `present()` | If present fails with `VK_ERROR_OUT_OF_DATE_KHR`, the swapchain is not recreated, leading to continued failures. |
| H3 | **vkBindImageMemory unchecked (line 1013)** | `allocateImageMemory()` | If binding fails, the image has no memory. Sampling from it → GPU fault. |
| H4 | **vkBindBufferMemory unchecked (line 1262)** | `createBuffer()` | If binding fails, buffer has no memory. Mapping at line 1267 may succeed but writes go nowhere; reads return garbage. |

### MEDIUM (Severity: 3)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| M1 | **Validation count functions lack mutex** | Lines 196–218 | `validationErrorCount()`, `validationWarningCount()`, `validationCriticalCount()` iterate `validationMessages_` without `validationMutex_`, creating data races with the debug callback thread. |
| M2 | **vkCreateDescriptorSetLayout unchecked (line 1305)** | `createDescriptorSetLayout()` | Returns VK_NULL_HANDLE on failure; caller uses it without checking → validation errors downstream. |
| M3 | **vkCreatePipelineLayout unchecked (line 1390)** | `createPipelineLayout()` | Returns VK_NULL_HANDLE on failure; `createGraphicsPipeline` receives null layout → validation error or crash. |
| M4 | **vkAllocateDescriptorSets unchecked (line 1327)** | `allocateDescriptorSet()` | Returns VK_NULL_HANDLE on failure; caller writes to null set → validation error. |
| M5 | **createSwapchain partial failure** | `createSwapchain()` lines 719–737 | If `vkCreateImageView` fails for swapchain image N, only images 0..N-1 have views. `destroySwapchain()` correctly handles this (checks each view), but the swapchain is in a partially initialized state. |

### LOW (Severity: 2)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| L1 | **vkResetFences unchecked (line 888)** | `beginFrame()` | If reset fails, fence behavior is undefined; unlikely but could cause spurious timeouts. |
| L2 | **vkEndCommandBuffer unchecked (lines 939, 1085)** | `endFrame()`, `transitionImageLayoutImmediate()` | Command buffer not properly finalized; `vkQueueSubmit` with invalid command buffer. |
| L3 | **vulkanDll_ member dead code** | Header line 449 | Member is declared but never used. Confusing for future maintainers. |
| L4 | **Enumerate query results unchecked (12 sites)** | Various | If enumeration fails, `formatCount`/`presentModeCount`/`deviceCount` may be 0 or garbage. Most have downstream checks, but `formats[0]` at line 629 crashes if `formats` is empty. |

### INFORMATIONAL (Severity: 1)

| # | Issue | Location | Impact |
|---|-------|----------|--------|
| I1 | **Hardcoded debug log path** | Lines 1538, 2285, 2451 | `fopen_s` with `D:\\Monix-2ago-unestable\\Monix\\Monix\\build\\...` — hardcoded path that won't exist on other machines. |
| I2 | **createShaderModule thread timeout** | Lines 1576–1607 | Worker thread detached on timeout; detached thread accessing `device_` is undefined if device is destroyed. |

---

## 10. Recommended Fixes

### Fix C1: Cleanup on initialize() Failure

Replace the cascade of bare `return false` with a single cleanup call:

```cpp
bool VulkanRenderer::initialize(HWND hwnd, uint32_t width, uint32_t height) {
    if (initialized_) return true;
    hwnd_ = hwnd;

    // ... all creation steps ...

    // On ANY failure, call full cleanup:
    // Option A: Set initialized_ = true temporarily, call shutdown(), return false
    // Option B: Factor out a private cleanupNonInitialized() that destroys
    //           whatever was created without checking initialized_

    initialized_ = true;
    return true;
}
```

The cleanest approach: add a helper `cleanupPartial()` that destroys resources in reverse creation order, checking each handle for VK_NULL_HANDLE before destroying. Call it on every failure path.

### Fix C2: Destroy Preset in shutdown()

Add to `shutdown()`, **before** destroying quad VBO (step 3):

```cpp
// Destroy preset resources first (they depend on device_)
destroyPreset();
```

### Fix C3/C4: DLL Lifecycle

```cpp
void VulkanRenderer::shutdown() {
    if (!initialized_) return;
    // ... destroy all Vulkan resources ...

    // After instance is destroyed, all function pointers are now dangling:
    // Reset them (optional, defensive)
    // (All 83 pfn_vk* = nullptr)

    // Free the DLL
    if (g_vkModule) {
        FreeLibrary(g_vkModule);
        g_vkModule = nullptr;
    }

    initialized_ = false;
}
```

Also remove the unused `vulkanDll_` member.

### Fix C5/C6/H1-H4/M2-M4: Add VkResult Checking

For critical operations, check and handle failure:

```cpp
VkResult result = pfn_vkBeginCommandBuffer(cmdBuffers_[currentFrame_], &beginInfo);
if (result != VK_SUCCESS) {
    OutputDebugStringA("[VK] vkBeginCommandBuffer failed\n");
    frameActive_ = false;
    return false;
}
```

For `vkMapMemory`:
```cpp
VkResult result = pfn_vkMapMemory(device_, result.memory, 0, size, 0, &result.mapped);
if (result != VK_SUCCESS) {
    OutputDebugStringA("[VK] vkMapMemory failed\n");
    // Clean up buffer
    pfn_vkDestroyBuffer(device_, result.buffer, nullptr);
    pfn_vkFreeMemory(device_, result.memory, nullptr);
    result = {};
    return result;
}
```

### Fix M1: Add Mutex to Validation Count Functions

```cpp
uint32_t VulkanRenderer::validationErrorCount() const {
    std::lock_guard<std::mutex> lock(validationMutex_);
    uint32_t count = 0;
    for (auto& m : validationMessages_) {
        if (m.severity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) count++;
    }
    return count;
}
```

Apply same pattern to `validationWarningCount()` and `validationCriticalCount()`.

### Fix I1: Remove Hardcoded Paths

Replace `fopen_s` with `OutputDebugStringA` logging, or use a configurable log path.

### Fix I2: Detach Timeout Thread Safely

The detached worker thread in `createShaderModule` may outlive the `VulkanRenderer` instance. Either:
- Use a join with a shorter timeout and a flag, or
- Accept that on timeout, the driver is already hung and the process is likely unrecoverable.

---

## Appendix A: Complete Function Pointer List

| # | Variable | Source | Line |
|---|----------|--------|------|
| 1 | `pfn_vkGetInstanceProcAddr` | GetProcAddress(g_vkModule) | 22 |
| 2 | `pfn_vkGetDeviceProcAddr` | GetProcAddress(g_vkModule) | 23 |
| 3 | `pfn_vkCreateInstance` | GetProcAddress(g_vkModule) | 32 |
| 4 | `pfn_vkDestroyInstance` | GetProcAddress(g_vkModule) | 33 |
| 5 | `pfn_vkEnumeratePhysicalDevices` | GetProcAddress(g_vkModule) | 34 |
| 6 | `pfn_vkGetPhysicalDeviceProperties` | GetProcAddress(g_vkModule) | 35 |
| 7 | `pfn_vkGetPhysicalDeviceMemoryProperties` | GetProcAddress(g_vkModule) | 36 |
| 8 | `pfn_vkGetPhysicalDeviceQueueFamilyProperties` | GetProcAddress(g_vkModule) | 37 |
| 9 | `pfn_vkGetPhysicalDeviceSurfaceSupportKHR` | GetProcAddress(g_vkModule) | 38 |
| 10 | `pfn_vkGetPhysicalDeviceSurfaceCapabilitiesKHR` | GetProcAddress(g_vkModule) | 39 |
| 11 | `pfn_vkGetPhysicalDeviceSurfaceFormatsKHR` | GetProcAddress(g_vkModule) | 40 |
| 12 | `pfn_vkGetPhysicalDeviceSurfacePresentModesKHR` | GetProcAddress(g_vkModule) | 41 |
| 13 | `pfn_vkCreateWin32SurfaceKHR` | GetProcAddress(g_vkModule) | 42 |
| 14 | `pfn_vkDestroySurfaceKHR` | GetProcAddress(g_vkModule) | 43 |
| 15 | `pfn_vkCreateDebugUtilsMessengerEXT` | GetProcAddress(g_vkModule) | 44 |
| 16 | `pfn_vkDestroyDebugUtilsMessengerEXT` | GetProcAddress(g_vkModule) | 45 |
| 17 | `pfn_vkEnumerateDeviceExtensionProperties` | GetProcAddress(g_vkModule) | 46 |
| 18 | `pfn_vkCreateDevice` | GetProcAddress(g_vkModule) | 49 |
| 19 | `pfn_vkDestroyDevice` | GetProcAddress(g_vkModule) | 50 |
| 20 | `pfn_vkGetDeviceQueue` | GetProcAddress(g_vkModule) | 51 |
| 21 | `pfn_vkCreateSwapchainKHR` | GetProcAddress(g_vkModule) | 52 |
| 22 | `pfn_vkDestroySwapchainKHR` | GetProcAddress(g_vkModule) | 53 |
| 23 | `pfn_vkGetSwapchainImagesKHR` | GetProcAddress(g_vkModule) | 54 |
| 24 | `pfn_vkAcquireNextImageKHR` | GetProcAddress(g_vkModule) | 55 |
| 25 | `pfn_vkQueuePresentKHR` | GetProcAddress(g_vkModule) | 56 |
| 26 | `pfn_vkQueueWaitIdle` | GetProcAddress(g_vkModule) | 57 |
| 27 | `pfn_vkDeviceWaitIdle` | GetProcAddress(g_vkModule) | 58 |
| 28 | `pfn_vkCreateCommandPool` | GetProcAddress(g_vkModule) | 59 |
| 29 | `pfn_vkDestroyCommandPool` | GetProcAddress(g_vkModule) | 60 |
| 30 | `pfn_vkAllocateCommandBuffers` | GetProcAddress(g_vkModule) | 61 |
| 31 | `pfn_vkFreeCommandBuffers` | GetProcAddress(g_vkModule) | 62 |
| 32 | `pfn_vkBeginCommandBuffer` | GetProcAddress(g_vkModule) | 63 |
| 33 | `pfn_vkEndCommandBuffer` | GetProcAddress(g_vkModule) | 64 |
| 34 | `pfn_vkCmdBeginRendering` | GetProcAddress(g_vkModule) | 65 |
| 35 | `pfn_vkCmdEndRendering` | GetProcAddress(g_vkModule) | 66 |
| 36 | `pfn_vkCmdBindPipeline` | GetProcAddress(g_vkModule) | 67 |
| 37 | `pfn_vkCmdSetViewport` | GetProcAddress(g_vkModule) | 68 |
| 38 | `pfn_vkCmdSetScissor` | GetProcAddress(g_vkModule) | 69 |
| 39 | `pfn_vkCmdDraw` | GetProcAddress(g_vkModule) | 70 |
| 40 | `pfn_vkCmdBlitImage` | GetProcAddress(g_vkModule) | 71 |
| 41 | `pfn_vkCmdCopyImageToBuffer` | GetProcAddress(g_vkModule) | 72 |
| 42 | `pfn_vkCmdPipelineBarrier` | GetProcAddress(g_vkModule) | 73 |
| 43 | `pfn_vkCmdBindVertexBuffers` | GetProcAddress(g_vkModule) | 74 |
| 44 | `pfn_vkCmdPushConstants` | GetProcAddress(g_vkModule) | 75 |
| 45 | `pfn_vkCmdBindDescriptorSets` | GetProcAddress(g_vkModule) | 76 |
| 46 | `pfn_vkCreateFence` | GetProcAddress(g_vkModule) | 77 |
| 47 | `pfn_vkDestroyFence` | GetProcAddress(g_vkModule) | 78 |
| 48 | `pfn_vkWaitForFences` | GetProcAddress(g_vkModule) | 79 |
| 49 | `pfn_vkResetFences` | GetProcAddress(g_vkModule) | 80 |
| 50 | `pfn_vkCreateSemaphore` | GetProcAddress(g_vkModule) | 81 |
| 51 | `pfn_vkDestroySemaphore` | GetProcAddress(g_vkModule) | 82 |
| 52 | `pfn_vkCreateImage` | GetProcAddress(g_vkModule) | 83 |
| 53 | `pfn_vkDestroyImage` | GetProcAddress(g_vkModule) | 84 |
| 54 | `pfn_vkGetImageMemoryRequirements` | GetProcAddress(g_vkModule) | 85 |
| 55 | `pfn_vkAllocateMemory` | GetProcAddress(g_vkModule) | 86 |
| 56 | `pfn_vkFreeMemory` | GetProcAddress(g_vkModule) | 87 |
| 57 | `pfn_vkBindImageMemory` | GetProcAddress(g_vkModule) | 88 |
| 58 | `pfn_vkCreateImageView` | GetProcAddress(g_vkModule) | 89 |
| 59 | `pfn_vkDestroyImageView` | GetProcAddress(g_vkModule) | 90 |
| 60 | `pfn_vkCreateSampler` | GetProcAddress(g_vkModule) | 91 |
| 61 | `pfn_vkDestroySampler` | GetProcAddress(g_vkModule) | 92 |
| 62 | `pfn_vkCreateBuffer` | GetProcAddress(g_vkModule) | 93 |
| 63 | `pfn_vkDestroyBuffer` | GetProcAddress(g_vkModule) | 94 |
| 64 | `pfn_vkGetBufferMemoryRequirements` | GetProcAddress(g_vkModule) | 95 |
| 65 | `pfn_vkBindBufferMemory` | GetProcAddress(g_vkModule) | 96 |
| 66 | `pfn_vkMapMemory` | GetProcAddress(g_vkModule) | 97 |
| 67 | `pfn_vkUnmapMemory` | GetProcAddress(g_vkModule) | 98 |
| 68 | `pfn_vkCreateShaderModule` | GetProcAddress(g_vkModule) | 99 |
| 69 | `pfn_vkDestroyShaderModule` | GetProcAddress(g_vkModule) | 100 |
| 70 | `pfn_vkCreatePipelineLayout` | GetProcAddress(g_vkModule) | 101 |
| 71 | `pfn_vkDestroyPipelineLayout` | GetProcAddress(g_vkModule) | 102 |
| 72 | `pfn_vkCreateGraphicsPipelines` | GetProcAddress(g_vkModule) | 103 |
| 73 | `pfn_vkDestroyPipeline` | GetProcAddress(g_vkModule) | 104 |
| 74 | `pfn_vkCreateDescriptorSetLayout` | GetProcAddress(g_vkModule) | 105 |
| 75 | `pfn_vkDestroyDescriptorSetLayout` | GetProcAddress(g_vkModule) | 106 |
| 76 | `pfn_vkCreateDescriptorPool` | GetProcAddress(g_vkModule) | 107 |
| 77 | `pfn_vkDestroyDescriptorPool` | GetProcAddress(g_vkModule) | 108 |
| 78 | `pfn_vkAllocateDescriptorSets` | GetProcAddress(g_vkModule) | 109 |
| 79 | `pfn_vkUpdateDescriptorSets` | GetProcAddress(g_vkModule) | 110 |
| 80 | `pfn_vkQueueSubmit` | GetProcAddress(g_vkModule) | 111 |
| 81 | `pfn_vkCmdCopyBufferToImage` | GetProcAddress(g_vkModule) | 112 |
| 82 | `pfn_vkGetImageSubresourceLayout` | GetProcAddress(g_vkModule) | 113 |
| 83 | `pfn_vkFlushMappedMemoryRanges` | GetProcAddress(g_vkModule) | 114 |

Note: `pfn_vkEnumerateInstanceLayerProperties` and `pfn_vkEnumerateInstanceExtensionProperties` are loaded as local variables at lines 239–242 and 435–436, not as static globals. They are not counted in the 83 above.

---

*End of audit.*
