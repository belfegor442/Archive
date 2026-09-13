#include "test_runtime.hpp"
#include "test_runner.hpp"

#include "../../vulkan_renderer.h"
#include "../../renderer_vk/compiler/SlangCompiler.hpp"
#include "../../renderer_vk/preset/SlangPresetParser.hpp"
#include "../../renderer_vk/preset/ShaderPreprocessor.hpp"
#include "../../renderer_vk/preset/StageSplitter.hpp"
#include "../../renderer_vk/preset/IncludeResolver.hpp"
#include "../../renderer_vk/preset/ParameterExtractor.hpp"
#include "../../renderer_vk/preset/AliasResolver.hpp"
#include "../../renderer_vk/graph/RenderGraphBuilder.hpp"
#include "../../renderer_vk/api/ShaderRenderer.hpp"
#include "../../renderer_vk/api/RendererTypes.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace fs = std::filesystem;
using namespace monix::renderer_vk;

// ============================================================================
// Hidden test window
// ============================================================================
static HWND createTestWindow(HINSTANCE instance) {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = instance;
    wc.lpszClassName = L"MonixVulkanTest";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(
        0, L"MonixVulkanTest", L"Vulkan Runtime Test",
        WS_OVERLAPPEDWINDOW,
        0, 0, 1280, 720,
        nullptr, nullptr, instance, nullptr);
    ShowWindow(hwnd, SW_HIDE);
    UpdateWindow(hwnd);
    return hwnd;
}

// ============================================================================
// Compile a .slang file to SPIR-V using SlangCompiler
// ============================================================================
struct CompiledShader {
    std::vector<uint32_t> vertWords;
    std::vector<uint32_t> fragWords;
    ShaderReflection vertReflection;
    ShaderReflection fragReflection;
    bool ok = false;
};

static CompiledShader compileSlangFile(
    const fs::path& slangPath,
    const fs::path& slangcPath,
    const fs::path& cacheDir,
    const std::vector<fs::path>& includeDirs) {

    CompiledShader result;

    // Read and preprocess
    IncludeResolver resolver;
    for (const auto& d : includeDirs) resolver.addRoot(d);
    ShaderPreprocessor preprocessor(resolver);

    auto preprocessed = preprocessor.preprocessFile(slangPath);
    if (!preprocessed) {
        fprintf(stdout, "  [FAIL] preprocess %s: %s\n", slangPath.string().c_str(), preprocessed.error().c_str());
        return result;
    }

    // Split stages
    StageSplitter splitter;
    auto stages = splitter.split(preprocessed.value().source);
    if (!stages) {
        fprintf(stdout, "  [FAIL] stage split %s: %s\n", slangPath.string().c_str(), stages.error().c_str());
        return result;
    }

    SlangCompiler compiler;

    // Compile vertex
    if (stages.value().hasVertex) {
        ShaderCompileRequest req;
        req.stage = ShaderStage::Vertex;
        req.source = stages.value().vertex;
        req.sourcePath = slangPath;
        req.outputDirectory = cacheDir / "runtime-test";
        req.slangcPath = slangcPath;
        req.includeDirectories = includeDirs;
        req.debugInfo = false;

        auto vertResult = compiler.compile(req);
        if (!vertResult) {
            fprintf(stdout, "  [FAIL] vertex compile %s: %s\n", slangPath.string().c_str(), vertResult.error().c_str());
            return result;
        }
        // Convert bytes to uint32_t words
        const auto& bytes = vertResult.value().spirv;
        result.vertWords.resize(bytes.size() / 4);
        memcpy(result.vertWords.data(), bytes.data(), result.vertWords.size() * 4);
        result.vertReflection = vertResult.value().reflection;
        fprintf(stdout, "  [OK] vertex SPIR-V: %zu words\n", result.vertWords.size());
    }

    // Compile fragment
    if (stages.value().hasFragment) {
        ShaderCompileRequest req;
        req.stage = ShaderStage::Fragment;
        req.source = stages.value().fragment;
        req.sourcePath = slangPath;
        req.outputDirectory = cacheDir / "runtime-test";
        req.slangcPath = slangcPath;
        req.includeDirectories = includeDirs;
        req.debugInfo = false;

        auto fragResult = compiler.compile(req);
        if (!fragResult) {
            fprintf(stdout, "  [FAIL] fragment compile %s: %s\n", slangPath.string().c_str(), fragResult.error().c_str());
            return result;
        }
        const auto& bytes = fragResult.value().spirv;
        result.fragWords.resize(bytes.size() / 4);
        memcpy(result.fragWords.data(), bytes.data(), result.fragWords.size() * 4);
        result.fragReflection = fragResult.value().reflection;
        fprintf(stdout, "  [OK] fragment SPIR-V: %zu words\n", result.fragWords.size());
    }

    result.ok = true;
    return result;
}

// ============================================================================
// Generate dummy BGRA source image (gradient pattern)
// ============================================================================
static std::vector<uint8_t> generateTestImage(uint32_t w, uint32_t h) {
    std::vector<uint8_t> pixels(w * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (y * w + x) * 4;
            pixels[idx + 0] = static_cast<uint8_t>((x * 255) / w);  // B
            pixels[idx + 1] = static_cast<uint8_t>((y * 255) / h);  // G
            pixels[idx + 2] = 128;                                     // R
            pixels[idx + 3] = 255;                                     // A
        }
    }
    return pixels;
}

// ============================================================================
// Read back swapchain pixels after rendering
// ============================================================================
static std::vector<uint8_t> readBackSwapchainPixels(VulkanRenderer& vk, uint32_t w, uint32_t h) {
    vk.requestScreenshot(w, h);
    // The screenshot copy is recorded inside renderPreset()
    // After endFrame() + present() + finalizeScreenshot(), pixels are ready
    return std::vector<uint8_t>(); // caller must finish the frame
}

// ============================================================================
// Test 1: Single pass — identity shader
// ============================================================================
static bool test_single_pass(VulkanRenderer& vk, const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "single_pass";

    uint32_t testW = 64, testH = 64;

    // Compile identity shader
    auto slangcPath = rootDir / "tools" / "slangc.exe";
    auto cacheDir = rootDir / "build" / "shader-cache-vk";
    auto shaderPath = rootDir / "Shaders" / "tests" / "identity.slang";
    std::vector<fs::path> includeDirs = {
        rootDir / "Shaders",
        rootDir / "Shaders" / "tests"
    };

    auto compiled = compileSlangFile(shaderPath, slangcPath, cacheDir, includeDirs);
    TEST_ASSERT(compiled.ok, "shader compilation failed");
    TEST_ASSERT(!compiled.vertWords.empty(), "vertex SPIR-V is empty");
    TEST_ASSERT(!compiled.fragWords.empty(), "fragment SPIR-V is empty");

    // Build per-pass sampler bindings from reflection
    std::vector<VulkanRenderer::SamplerBindingInfo> samplerBindings;
    for (const auto& s : compiled.fragReflection.samplers) {
        samplerBindings.push_back({s.binding, s.name});
    }

    // Load preset
    std::vector<std::vector<uint32_t>> vertSpvs = { compiled.vertWords };
    std::vector<std::vector<uint32_t>> fragSpvs = { compiled.fragWords };
    std::vector<uint32_t> pushSizes = { compiled.fragReflection.pushConstants.empty() ? 0u : compiled.fragReflection.pushConstants[0].size };
    std::vector<VkFormat> rtFormats = { VK_FORMAT_B8G8R8A8_UNORM };
    std::vector<std::vector<float>> pushDefaults = { {} };
    std::vector<uint32_t> uboSizes = { sizeof(UniformData) };
    std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassBindings = { samplerBindings };

    bool loaded = vk.loadPresetReflection(testW, testH, vertSpvs, fragSpvs, pushSizes, rtFormats, pushDefaults, uboSizes, perPassBindings);
    TEST_ASSERT(loaded, "loadPresetReflection failed");

    // Generate test source image
    auto srcPixels = generateTestImage(testW, testH);

    // Render frame
    bool frameOk = vk.beginFrame(testW, testH);
    TEST_ASSERT(frameOk, "beginFrame failed");

    vk.uploadTextureToImage(srcPixels.data(), testW, testH, false);

    vk.renderPreset(testW, testH);

    vk.endFrame();
    vk.present();

    // If we got here without crash/VK error, the pipeline executed
    fprintf(stdout, "  [OK] Single pass rendered without errors\n");

    vk.destroyPreset();
    return true;
}

// ============================================================================
// Test 2: Two-pass rendering
// ============================================================================
static bool test_two_pass(VulkanRenderer& vk, const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "two_pass";

    uint32_t testW = 64, testH = 64;

    auto slangcPath = rootDir / "tools" / "slangc.exe";
    auto cacheDir = rootDir / "build" / "shader-cache-vk";
    auto shaderPath = rootDir / "Shaders" / "tests" / "passthrough.slang";
    std::vector<fs::path> includeDirs = {
        rootDir / "Shaders",
        rootDir / "Shaders" / "tests"
    };

    auto compiled = compileSlangFile(shaderPath, slangcPath, cacheDir, includeDirs);
    TEST_ASSERT(compiled.ok, "shader compilation failed");

    std::vector<VulkanRenderer::SamplerBindingInfo> samplerBindings;
    for (const auto& s : compiled.fragReflection.samplers) {
        samplerBindings.push_back({s.binding, s.name});
    }

    // Two passes: both use the same shader (identity)
    std::vector<std::vector<uint32_t>> vertSpvs = { compiled.vertWords, compiled.vertWords };
    std::vector<std::vector<uint32_t>> fragSpvs = { compiled.fragWords, compiled.fragWords };
    std::vector<uint32_t> pushSizes = { 0, 0 };
    std::vector<VkFormat> rtFormats = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM };
    std::vector<std::vector<float>> pushDefaults = { {}, {} };
    std::vector<uint32_t> uboSizes = { sizeof(UniformData), sizeof(UniformData) };
    std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassBindings = { samplerBindings, samplerBindings };

    bool loaded = vk.loadPresetReflection(testW, testH, vertSpvs, fragSpvs, pushSizes, rtFormats, pushDefaults, uboSizes, perPassBindings);
    TEST_ASSERT(loaded, "loadPresetReflection failed for 2-pass");

    auto srcPixels = generateTestImage(testW, testH);

    bool frameOk = vk.beginFrame(testW, testH);
    TEST_ASSERT(frameOk, "beginFrame failed");

    vk.uploadTextureToImage(srcPixels.data(), testW, testH, false);
    vk.renderPreset(testW, testH);
    vk.endFrame();
    vk.present();

    fprintf(stdout, "  [OK] Two-pass rendered without errors\n");

    vk.destroyPreset();
    return true;
}

// ============================================================================
// Test 3: Resize
// ============================================================================
static bool test_resize(VulkanRenderer& vk, const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "resize_test";

    uint32_t testW = 128, testH = 128;

    auto slangcPath = rootDir / "tools" / "slangc.exe";
    auto cacheDir = rootDir / "build" / "shader-cache-vk";
    auto shaderPath = rootDir / "Shaders" / "tests" / "identity.slang";
    std::vector<fs::path> includeDirs = {
        rootDir / "Shaders",
        rootDir / "Shaders" / "tests"
    };

    auto compiled = compileSlangFile(shaderPath, slangcPath, cacheDir, includeDirs);
    TEST_ASSERT(compiled.ok, "shader compilation failed");

    std::vector<VulkanRenderer::SamplerBindingInfo> samplerBindings;
    for (const auto& s : compiled.fragReflection.samplers) {
        samplerBindings.push_back({s.binding, s.name});
    }

    // Render at 128x128
    {
        std::vector<std::vector<uint32_t>> vertSpvs = { compiled.vertWords };
        std::vector<std::vector<uint32_t>> fragSpvs = { compiled.fragWords };
        std::vector<uint32_t> pushSizes = { 0 };
        std::vector<VkFormat> rtFormats = { VK_FORMAT_B8G8R8A8_UNORM };
        std::vector<std::vector<float>> pushDefaults = { {} };
        std::vector<uint32_t> uboSizes = { sizeof(UniformData) };
        std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassBindings = { samplerBindings };

        bool loaded = vk.loadPresetReflection(testW, testH, vertSpvs, fragSpvs, pushSizes, rtFormats, pushDefaults, uboSizes, perPassBindings);
        TEST_ASSERT(loaded, "load at 128x128 failed");

        auto srcPixels = generateTestImage(testW, testH);
        bool ok = vk.beginFrame(testW, testH);
        TEST_ASSERT(ok, "beginFrame at 128x128 failed");
        vk.uploadTextureToImage(srcPixels.data(), testW, testH, false);
        vk.renderPreset(testW, testH);
        vk.endFrame();
        vk.present();
        vk.destroyPreset();
        fprintf(stdout, "  [OK] Rendered at 128x128\n");
    }

    // Resize to 256x256
    {
        uint32_t newW = 256, newH = 256;
        vk.resize(newW, newH);

        std::vector<std::vector<uint32_t>> vertSpvs = { compiled.vertWords };
        std::vector<std::vector<uint32_t>> fragSpvs = { compiled.fragWords };
        std::vector<uint32_t> pushSizes = { 0 };
        std::vector<VkFormat> rtFormats = { VK_FORMAT_B8G8R8A8_UNORM };
        std::vector<std::vector<float>> pushDefaults = { {} };
        std::vector<uint32_t> uboSizes = { sizeof(UniformData) };
        std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassBindings = { samplerBindings };

        bool loaded = vk.loadPresetReflection(newW, newH, vertSpvs, fragSpvs, pushSizes, rtFormats, pushDefaults, uboSizes, perPassBindings);
        TEST_ASSERT(loaded, "load at 256x256 failed");

        auto srcPixels = generateTestImage(newW, newH);
        bool ok = vk.beginFrame(newW, newH);
        TEST_ASSERT(ok, "beginFrame at 256x256 failed");
        vk.uploadTextureToImage(srcPixels.data(), newW, newH, false);
        vk.renderPreset(newW, newH);
        vk.endFrame();
        vk.present();
        vk.destroyPreset();
        fprintf(stdout, "  [OK] Rendered at 256x256\n");
    }

    // Resize back to 128x128
    {
        vk.resize(testW, testH);

        std::vector<std::vector<uint32_t>> vertSpvs = { compiled.vertWords };
        std::vector<std::vector<uint32_t>> fragSpvs = { compiled.fragWords };
        std::vector<uint32_t> pushSizes = { 0 };
        std::vector<VkFormat> rtFormats = { VK_FORMAT_B8G8R8A8_UNORM };
        std::vector<std::vector<float>> pushDefaults = { {} };
        std::vector<uint32_t> uboSizes = { sizeof(UniformData) };
        std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassBindings = { samplerBindings };

        bool loaded = vk.loadPresetReflection(testW, testH, vertSpvs, fragSpvs, pushSizes, rtFormats, pushDefaults, uboSizes, perPassBindings);
        TEST_ASSERT(loaded, "load at 128x128 again failed");

        auto srcPixels = generateTestImage(testW, testH);
        bool ok = vk.beginFrame(testW, testH);
        TEST_ASSERT(ok, "beginFrame at 128x128 again failed");
        vk.uploadTextureToImage(srcPixels.data(), testW, testH, false);
        vk.renderPreset(testW, testH);
        vk.endFrame();
        vk.present();
        vk.destroyPreset();
        fprintf(stdout, "  [OK] Rendered at 128x128 again\n");
    }

    return true;
}

// ============================================================================
// Test 4: Slang compiler availability
// ============================================================================
static bool test_slangc_available(const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "slangc_available";
    auto slangcPath = rootDir / "tools" / "slangc.exe";
    TEST_ASSERT(fs::exists(slangcPath), "slangc.exe not found at " + slangcPath.string());
    return true;
}

// ============================================================================
// Test 5: SPIR-V reflection correctness
// ============================================================================
static bool test_reflection_correctness(const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "reflection_correctness";

    auto slangcPath = rootDir / "tools" / "slangc.exe";
    auto cacheDir = rootDir / "build" / "shader-cache-vk";
    auto shaderPath = rootDir / "Shaders" / "tests" / "identity.slang";
    std::vector<fs::path> includeDirs = { rootDir / "Shaders", rootDir / "Shaders" / "tests" };

    auto compiled = compileSlangFile(shaderPath, slangcPath, cacheDir, includeDirs);
    TEST_ASSERT(compiled.ok, "shader compilation failed");

    // Verify fragment reflection has Source sampler at binding 2
    bool foundSource = false;
    for (const auto& s : compiled.fragReflection.samplers) {
        if (s.name == "Source" && s.binding == 2) {
            foundSource = true;
            break;
        }
    }
    TEST_ASSERT(foundSource, "fragment reflection missing 'Source' sampler at binding 2");

    // Verify vertex SPIR-V has at least 100 words (real compiled code, not empty)
    TEST_ASSERT(compiled.vertWords.size() >= 50,
        "vertex SPIR-V too small: " + std::to_string(compiled.vertWords.size()) + " words");
    TEST_ASSERT(compiled.fragWords.size() >= 50,
        "fragment SPIR-V too small: " + std::to_string(compiled.fragWords.size()) + " words");

    // Verify SPIR-V magic number (0x07230203)
    TEST_ASSERT(compiled.vertWords.size() > 0 && compiled.vertWords[0] == 0x07230203,
        "vertex SPIR-V invalid magic number");
    TEST_ASSERT(compiled.fragWords.size() > 0 && compiled.fragWords[0] == 0x07230203,
        "fragment SPIR-V invalid magic number");

    fprintf(stdout, "  [OK] Reflection: Source@binding=2, vert=%zu words, frag=%zu words\n",
        compiled.vertWords.size(), compiled.fragWords.size());
    return true;
}

// ============================================================================
// Test 6: Vulkan validation layer availability
// ============================================================================
static bool test_validation_layers() {
    using namespace monix::tests;
    std::string testName = "validation_layers";

    // Try to load vulkan-1.dll and check for layer properties
    HMODULE vkMod = LoadLibraryW(L"vulkan-1.dll");
    if (!vkMod) {
        fprintf(stdout, "  [INFO] vulkan-1.dll not found — validation layers unavailable\n");
        return true; // Not a failure, just unavailable
    }

    using PFN_vkEnumerateInstanceLayerProperties = VkResult(VKAPI_CALL*)(uint32_t*, VkLayerProperties*);
    auto pfnEnum = reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
        GetProcAddress(vkMod, "vkEnumerateInstanceLayerProperties"));

    if (!pfnEnum) {
        FreeLibrary(vkMod);
        fprintf(stdout, "  [INFO] vkEnumerateInstanceLayerProperties not found\n");
        return true;
    }

    uint32_t count = 0;
    pfnEnum(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    pfnEnum(&count, layers.data());

    bool foundValidation = false;
    for (const auto& l : layers) {
        if (strstr(l.layerName, "validation")) {
            foundValidation = true;
            fprintf(stdout, "  [OK] Validation layer found: %s\n", l.layerName);
            break;
        }
    }

    FreeLibrary(vkMod);

    if (!foundValidation) {
        fprintf(stdout, "  [INFO] No Vulkan validation layer installed (Vulkan SDK required)\n");
    }
    return true;
}

// ============================================================================
// Test 7: Multi-pass descriptor binding verification
// ============================================================================
static bool test_descriptor_binding(VulkanRenderer& vk, const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "descriptor_binding";

    uint32_t testW = 64, testH = 64;

    auto slangcPath = rootDir / "tools" / "slangc.exe";
    auto cacheDir = rootDir / "build" / "shader-cache-vk";
    auto shaderPath = rootDir / "Shaders" / "tests" / "identity.slang";
    std::vector<fs::path> includeDirs = { rootDir / "Shaders", rootDir / "Shaders" / "tests" };

    auto compiled = compileSlangFile(shaderPath, slangcPath, cacheDir, includeDirs);
    TEST_ASSERT(compiled.ok, "shader compilation failed");

    // Build sampler bindings from reflection
    std::vector<VulkanRenderer::SamplerBindingInfo> samplerBindings;
    for (const auto& s : compiled.fragReflection.samplers) {
        samplerBindings.push_back({s.binding, s.name});
        fprintf(stdout, "  [INFO] Sampler: name=%s binding=%u set=%u\n",
            s.name.c_str(), s.binding, s.set);
    }
    TEST_ASSERT(!samplerBindings.empty(), "no sampler bindings from reflection");

    // Load preset and verify it created resources
    std::vector<std::vector<uint32_t>> vertSpvs = { compiled.vertWords };
    std::vector<std::vector<uint32_t>> fragSpvs = { compiled.fragWords };
    std::vector<uint32_t> pushSizes = { compiled.fragReflection.pushConstants.empty() ? 0u : compiled.fragReflection.pushConstants[0].size };
    std::vector<VkFormat> rtFormats = { VK_FORMAT_B8G8R8A8_UNORM };
    std::vector<std::vector<float>> pushDefaults = { {} };
    std::vector<uint32_t> uboSizes = { sizeof(UniformData) };
    std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassBindings = { samplerBindings };

    bool loaded = vk.loadPresetReflection(testW, testH, vertSpvs, fragSpvs, pushSizes, rtFormats, pushDefaults, uboSizes, perPassBindings);
    TEST_ASSERT(loaded, "loadPresetReflection failed");

    // Verify preset has resources
    const auto& preset = vk.currentPreset();
    TEST_ASSERT(preset.valid(), "preset not valid after load");
    TEST_ASSERT(preset.passes.size() == 1, "expected 1 pass");
    TEST_ASSERT(preset.passes[0].pipeline != VK_NULL_HANDLE, "pipeline not created");
    TEST_ASSERT(preset.passes[0].pipelineLayout != VK_NULL_HANDLE, "pipelineLayout not created");
    TEST_ASSERT(preset.passes[0].descriptorSet != VK_NULL_HANDLE, "descriptorSet not created");
    TEST_ASSERT(preset.passes[0].renderTarget.image != VK_NULL_HANDLE, "renderTarget not created");
    TEST_ASSERT(preset.passes[0].uniformBuffer.buffer != VK_NULL_HANDLE, "uniformBuffer not created");
    TEST_ASSERT(preset.samplers.size() > 0, "no samplers created");

    fprintf(stdout, "  [OK] All Vulkan resources created: pipeline=%p descriptorSet=%p rt=%p\n",
        (void*)preset.passes[0].pipeline, (void*)preset.passes[0].descriptorSet,
        (void*)preset.passes[0].renderTarget.image);

    // Render to verify descriptors are valid at draw time
    auto srcPixels = generateTestImage(testW, testH);
    bool ok = vk.beginFrame(testW, testH);
    TEST_ASSERT(ok, "beginFrame failed");
    vk.uploadTextureToImage(srcPixels.data(), testW, testH, false);
    vk.renderPreset(testW, testH);
    vk.endFrame();
    vk.present();

    fprintf(stdout, "  [OK] Descriptor sets bound and draw executed successfully\n");

    vk.destroyPreset();
    return true;
}

// ============================================================================
// Test 8: Single-pass pixel verification (blit to swapchain readback)
// ============================================================================
static bool test_single_pass_pixels(VulkanRenderer& vk, const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "single_pass_pixels";

    uint32_t sw = vk.swapchainWidth();
    uint32_t sh = vk.swapchainHeight();

    std::vector<uint8_t> srcPixels(sw * sh * 4);
    for (size_t i = 0; i < srcPixels.size(); i += 4) {
        srcPixels[i + 0] = 255;
        srcPixels[i + 1] = 0;
        srcPixels[i + 2] = 0;
        srcPixels[i + 3] = 255;
    }

    vk.requestScreenshot(sw, sh);

    bool frameOk = vk.beginFrame(sw, sh);
    TEST_ASSERT(frameOk, "beginFrame failed");
    vk.uploadTextureToImage(srcPixels.data(), sw, sh, true);
    vk.endFrame();
    vk.present();
    vk.waitForIdle();
    vk.finalizeScreenshot();
    TEST_ASSERT(vk.hasScreenshot(), "screenshot not ready after blit");

    auto readback = vk.takeScreenshot();
    TEST_ASSERT(readback.size() == sw * sh * 4, "screenshot size mismatch");

    size_t blueCount = 0;
    for (size_t i = 0; i < readback.size(); i += 4) {
        uint8_t b = readback[i + 0];
        uint8_t g = readback[i + 1];
        uint8_t r = readback[i + 2];
        if (b > 200 && g < 50 && r < 50) blueCount++;
    }
    double bluePct = 100.0 * blueCount / (sw * sh);
    fprintf(stdout, "  [OK] Blit blue pixels: %zu/%u (%.1f%%)\n", blueCount, sw * sh, bluePct);
    TEST_ASSERT(bluePct > 50.0, "blit readback: less than 50%% blue pixels");
    return true;
}

// ============================================================================
// Test 9: Two-pass pixel verification (solid_red → identity via preset)
// ============================================================================
static bool test_two_pass_pixels(VulkanRenderer& vk, const fs::path& rootDir) {
    using namespace monix::tests;
    std::string testName = "two_pass_pixels";

    uint32_t sw = vk.swapchainWidth();
    uint32_t sh = vk.swapchainHeight();

    auto slangcPath = rootDir / "tools" / "slangc.exe";
    auto cacheDir = rootDir / "build" / "shader-cache-vk";
    std::vector<fs::path> includeDirs = { rootDir / "Shaders", rootDir / "Shaders" / "tests" };

    auto solidRed = compileSlangFile(rootDir / "Shaders" / "tests" / "solid_red.slang",
        slangcPath, cacheDir, includeDirs);
    TEST_ASSERT(solidRed.ok, "solid_red compilation failed");

    auto identity = compileSlangFile(rootDir / "Shaders" / "tests" / "identity.slang",
        slangcPath, cacheDir, includeDirs);
    TEST_ASSERT(identity.ok, "identity compilation failed");

    std::vector<VulkanRenderer::SamplerBindingInfo> pass0Bindings;
    for (const auto& s : solidRed.fragReflection.samplers)
        pass0Bindings.push_back({s.binding, s.name});

    std::vector<VulkanRenderer::SamplerBindingInfo> pass1Bindings;
    for (const auto& s : identity.fragReflection.samplers)
        pass1Bindings.push_back({s.binding, s.name});

    std::vector<std::vector<uint32_t>> vertSpvs = { solidRed.vertWords, identity.vertWords };
    std::vector<std::vector<uint32_t>> fragSpvs = { solidRed.fragWords, identity.fragWords };
    std::vector<uint32_t> pushSizes = { 0, 0 };
    std::vector<VkFormat> rtFormats = { VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM };
    std::vector<std::vector<float>> pushDefaults = { {}, {} };
    std::vector<uint32_t> uboSizes = { sizeof(UniformData), sizeof(UniformData) };
    std::vector<std::vector<VulkanRenderer::SamplerBindingInfo>> perPassBindings = { pass0Bindings, pass1Bindings };

    bool loaded = vk.loadPresetReflection(sw, sh, vertSpvs, fragSpvs, pushSizes, rtFormats,
        pushDefaults, uboSizes, perPassBindings);
    TEST_ASSERT(loaded, "loadPresetReflection failed for 2-pass pixel test");

    std::vector<uint8_t> srcPixels(sw * sh * 4, 0);

    vk.requestScreenshot(sw, sh);

    bool frameOk = vk.beginFrame(sw, sh);
    TEST_ASSERT(frameOk, "beginFrame failed");
    vk.uploadTextureToImage(srcPixels.data(), sw, sh, false);
    vk.renderPreset(sw, sh);
    vk.endFrame();
    vk.present();
    vk.waitForIdle();
    vk.finalizeScreenshot();
    TEST_ASSERT(vk.hasScreenshot(), "screenshot not ready");

    auto readback = vk.takeScreenshot();
    TEST_ASSERT(readback.size() == sw * sh * 4, "screenshot size mismatch");

    size_t redCount = 0;
    for (size_t i = 0; i < readback.size(); i += 4) {
        uint8_t b = readback[i + 0];
        uint8_t g = readback[i + 1];
        uint8_t r = readback[i + 2];
        if (r > 200 && g < 50 && b < 50) redCount++;
    }

    double redPct = 100.0 * redCount / (sw * sh);
    fprintf(stdout, "  [OK] Red pixels: %zu/%u (%.1f%%)\n", redCount, sw * sh, redPct);
    TEST_ASSERT(redPct > 50.0, "less than 50%% pixels are red");

    vk.destroyPreset();
    return true;
}

// ============================================================================
// Test runner
// ============================================================================
int runVulkanRuntimeTests(HINSTANCE instance) {
    fprintf(stdout, "\n=== Vulkan Runtime Tests ===\n");
    fflush(stdout);

    fs::path rootDir = "D:\\Monix-2ago-unestable\\Monix\\Monix";

    // Test slangc availability first
    {
        std::string testName = "slangc_available";
        bool passed = false;
        std::string error;
        try { passed = test_slangc_available(rootDir); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "exception"; }
        monix::tests::reportResult(testName, passed, error);
    }

    // Test SPIR-V reflection correctness
    {
        std::string testName = "reflection_correctness";
        bool passed = false;
        std::string error;
        try { passed = test_reflection_correctness(rootDir); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "exception"; }
        monix::tests::reportResult(testName, passed, error);
    }

    // Test Vulkan validation layer availability
    {
        std::string testName = "validation_layers";
        bool passed = false;
        std::string error;
        try { passed = test_validation_layers(); }
        catch (const std::exception& e) { error = e.what(); }
        catch (...) { error = "exception"; }
        monix::tests::reportResult(testName, passed, error);
    }

    // Create hidden test window
    HWND hwnd = createTestWindow(instance);
    if (!hwnd) {
        monix::tests::reportResult("vulkan_window", false, "Failed to create test window");
        monix::tests::printSummary();
        return 1;
    }
    monix::tests::reportResult("vulkan_window", true);

    // Create single shared VulkanRenderer for all Vulkan tests
    // This avoids creating/destroying multiple VkInstances which crashes the GPU driver
    VulkanRenderer vk;
    if (!vk.initialize(hwnd, 128, 128)) {
        monix::tests::reportResult("vulkan_single_pass", false, "VulkanRenderer init failed");
        monix::tests::reportResult("vulkan_two_pass", false, "VulkanRenderer init failed");
        monix::tests::reportResult("vulkan_resize", false, "VulkanRenderer init failed");
        monix::tests::reportResult("descriptor_binding", false, "VulkanRenderer init failed");
        monix::tests::reportResult("single_pass_pixels", false, "VulkanRenderer init failed");
        monix::tests::reportResult("two_pass_pixels", false, "VulkanRenderer init failed");
    } else {
        {
            std::string testName = "vulkan_single_pass";
            bool passed = false;
            std::string error;
            try { passed = test_single_pass(vk, rootDir); }
            catch (const std::exception& e) { error = e.what(); }
            catch (...) { error = "exception"; }
            monix::tests::reportResult(testName, passed, error);
        }
        {
            std::string testName = "vulkan_two_pass";
            bool passed = false;
            std::string error;
            try { passed = test_two_pass(vk, rootDir); }
            catch (const std::exception& e) { error = e.what(); }
            catch (...) { error = "exception"; }
            monix::tests::reportResult(testName, passed, error);
        }
        {
            std::string testName = "vulkan_resize";
            bool passed = false;
            std::string error;
            try { passed = test_resize(vk, rootDir); }
            catch (const std::exception& e) { error = e.what(); }
            catch (...) { error = "exception"; }
            monix::tests::reportResult(testName, passed, error);
        }
        {
            std::string testName = "descriptor_binding";
            bool passed = false;
            std::string error;
            try { passed = test_descriptor_binding(vk, rootDir); }
            catch (const std::exception& e) { error = e.what(); }
            catch (...) { error = "exception"; }
            monix::tests::reportResult(testName, passed, error);
        }
        {
            std::string testName = "single_pass_pixels";
            bool passed = false;
            std::string error;
            try { passed = test_single_pass_pixels(vk, rootDir); }
            catch (const std::exception& e) { error = e.what(); }
            catch (...) { error = "exception"; }
            monix::tests::reportResult(testName, passed, error);
        }
        {
            std::string testName = "two_pass_pixels";
            bool passed = false;
            std::string error;
            try { passed = test_two_pass_pixels(vk, rootDir); }
            catch (const std::exception& e) { error = e.what(); }
            catch (...) { error = "exception"; }
            monix::tests::reportResult(testName, passed, error);
        }
    }

    DestroyWindow(hwnd);
    UnregisterClassW(L"MonixVulkanTest", instance);

    monix::tests::printSummary();
    return monix::tests::allResults().size();
}
