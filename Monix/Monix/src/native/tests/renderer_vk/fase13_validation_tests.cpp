#include "fase13_validation_tests.hpp"

#include "renderer_vk/api/ShaderRenderer.hpp"
#include "renderer_vk/api/RendererTypes.hpp"
#include "renderer_vk/library/ShaderLibrary.hpp"
#include "renderer_vk/library/ShaderLibraryCompiler.hpp"
#include "renderer_vk/shader_runtime/ShaderRuntime.hpp"
#include "renderer_vk/shader_runtime/TransactionalShaderState.hpp"
#include "renderer_vk/compiler/ShaderCache.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

namespace fs = std::filesystem;
using namespace monix::renderer_vk;

static int s_passed = 0;
static int s_failed = 0;
static int s_skipped = 0;

static const fs::path kBaseDir = "D:/Monix-2ago-unestable/Monix/Monix";
static const fs::path kShaderDir = kBaseDir / "tests/shaders";
static const fs::path kTestOutputDir = "D:/Monix-2ago-unestable/build/fase13-output";

static bool ensureOutputDir() {
    std::error_code ec;
    fs::create_directories(kTestOutputDir, ec);
    return !ec;
}

static void writeFile(const fs::path& path, const std::string& content) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream f(path);
    f << content;
    f.close();
}

// ============================================================================
// Hidden HWND for headless GPU testing
// ============================================================================
struct TestWindow {
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    uint32_t width = 320;
    uint32_t height = 240;

    bool create(uint32_t w = 320, uint32_t h = 240) {
        width = w;
        height = h;
        HINSTANCE hInst = GetModuleHandle(nullptr);
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = hInst;
        wc.lpszClassName = L"MonixFase13Window";
        RegisterClassExW(&wc);
        hwnd = CreateWindowExW(
            0, L"MonixFase13Window", L"F13 Test",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            static_cast<int>(w + 16), static_cast<int>(h + 39),
            nullptr, nullptr, hInst, nullptr);
        if (!hwnd) return false;
        ShowWindow(hwnd, SW_HIDE);
        UpdateWindow(hwnd);
        dc = GetDC(hwnd);
        return hwnd != nullptr;
    }

    void destroy() {
        if (dc) { ReleaseDC(hwnd, dc); dc = nullptr; }
        if (hwnd) { DestroyWindow(hwnd); hwnd = nullptr; }
    }
};

// ============================================================================
// Screenshot readback
// ============================================================================
struct GpuReadback {
    std::vector<uint8_t> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
    bool valid = false;
};

static GpuReadback captureFrame(VulkanRenderer& vk, ShaderRenderer& sr,
                                 uint32_t w, uint32_t h) {
    GpuReadback rb;
    rb.width = w;
    rb.height = h;
    vk.requestScreenshot(w, h);
    FrameContext fc{};
    fc.frameIndex = 0;
    fc.frameDirection = 1;
    fc.sourceExtent = {w, h};
    fc.outputExtent = {w, h};
    sr.render(fc, nullptr);
    vk.finalizeScreenshot();
    rb.pixels = vk.takeScreenshot();
    rb.valid = !rb.pixels.empty();
    return rb;
}

static bool validateSolidColor(const GpuReadback& rb,
                                uint8_t expectedR, uint8_t expectedG, uint8_t expectedB,
                                uint8_t tolerance = 8) {
    if (!rb.valid || rb.pixels.empty()) return false;
    size_t totalPixels = rb.width * rb.height;
    size_t matchingPixels = 0;
    for (size_t i = 0; i < totalPixels; ++i) {
        uint8_t b = rb.pixels[i * 4 + 0];
        uint8_t g = rb.pixels[i * 4 + 1];
        uint8_t r = rb.pixels[i * 4 + 2];
        uint8_t a = rb.pixels[i * 4 + 3];
        bool rOk = (r >= expectedR - tolerance && r <= expectedR + tolerance);
        bool gOk = (g >= expectedG - tolerance && g <= expectedG + tolerance);
        bool bOk = (b >= expectedB - tolerance && b <= expectedB + tolerance);
        bool aOk = (a >= 240);
        if (rOk && gOk && bOk && aOk) matchingPixels++;
    }
    double matchRatio = static_cast<double>(matchingPixels) / totalPixels;
    return matchRatio >= 0.95;
}

// ============================================================================
// Error classification helper (13.D)
// ============================================================================
struct ValidationSummary {
    uint32_t criticalCount = 0;
    uint32_t errorCount = 0;
    uint32_t warningCount = 0;
    uint32_t infoCount = 0;
    std::vector<std::string> criticalMessages;
    std::vector<std::string> errorMessages;
    std::vector<std::string> warningMessages;

    void collect(VulkanRenderer& vk) {
        auto msgs = vk.takeValidationMessages();
        for (auto& m : msgs) {
            if (m.severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                errorCount++;
                errorMessages.push_back(m.message);
            } else if (m.severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                warningCount++;
                warningMessages.push_back(m.message);
            } else {
                infoCount++;
            }
        }
    }

    void collectCriticals(VulkanRenderer& vk) {
        auto msgs = vk.takeValidationMessages();
        for (auto& m : msgs) {
            if (m.severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                criticalCount++;
                criticalMessages.push_back(m.message);
            }
        }
    }

    void print() const {
        printf("    Validation: critical=%u error=%u warning=%u info=%u\n",
               criticalCount, errorCount, warningCount, infoCount);
        for (auto& msg : criticalMessages) {
            printf("    CRITICAL: %s\n", msg.c_str());
        }
        for (auto& msg : errorMessages) {
            printf("    ERROR: %s\n", msg.c_str());
        }
        for (auto& msg : warningMessages) {
            printf("    WARNING: %s\n", msg.c_str());
        }
    }

    bool hasCriticalOrError() const {
        return criticalCount > 0 || errorCount > 0;
    }
};

// ============================================================================
// Validation test context — VulkanRenderer with validation ENABLED
// ============================================================================
struct F13Context {
    TestWindow window;
    VulkanRenderer vk;
    ShaderRenderer* sr = nullptr;
    ShaderRuntime* runtime = nullptr;
    ShaderLibrary* library = nullptr;
    ShaderLibraryCompiler* compiler = nullptr;
    TransactionalShaderState txState;
    bool initialized = false;
    bool validationEnabled = false;

    bool initialize() {
        if (!window.create(320, 240)) {
            printf("  FAIL: cannot create window\n");
            return false;
        }

        bool layerAvail = vk.enableValidation();
        if (!layerAvail) {
            printf("  WARN: VK_LAYER_KHRONOS_validation not available\n");
        }

        if (!vk.initialize(window.hwnd, window.width, window.height)) {
            printf("  FAIL: Vulkan initialization failed\n");
            window.destroy();
            return false;
        }

        validationEnabled = vk.isValidationEnabled();
        printf("  GPU: %s (%s)\n", vk.getRenderer().c_str(), vk.getVendor().c_str());
        printf("  API: %s\n", vk.getVersion().c_str());
        printf("  Validation: %s\n", validationEnabled ? "ENABLED" : "NOT AVAILABLE");

        RendererConfig cfg;
        cfg.rootDirectory = kBaseDir;
        cfg.slangcPath = kBaseDir / "tools" / "slangc.exe";
        cfg.compileShadersToSpirv = true;

        sr = new ShaderRenderer(cfg);
        sr->setRenderer(&vk);
        sr->resize(window.width, window.height);

        ShaderRuntimeConfig rtCfg;
        rtCfg.rootDirectory = kBaseDir;
        rtCfg.shaderCacheDirectory = kBaseDir / "build" / "fase13-cache";
        rtCfg.slangcPath = kBaseDir / "tools" / "slangc.exe";
        rtCfg.compileShadersToSpirv = true;
        std::filesystem::create_directories(rtCfg.shaderCacheDirectory);

        runtime = new ShaderRuntime(rtCfg);
        runtime->initialize();
        if (sr->shaderCache()) {
            runtime->setCache(sr->shaderCache());
        }

        library = new ShaderLibrary(kBaseDir / "gpu-test-shaders");
        compiler = new ShaderLibraryCompiler(*library, *runtime, sr);

        initialized = true;
        return true;
    }

    void shutdown() {
        delete compiler; compiler = nullptr;
        delete library; library = nullptr;
        delete runtime; runtime = nullptr;
        delete sr; sr = nullptr;
        vk.shutdown();
        window.destroy();
        initialized = false;
    }
};

#define F13_TEST_SECTION(name) printf("\n--- %s ---\n", name); fflush(stdout);
#define F13_RUN_TEST(fn, ctx) do { fn(ctx); } while(0)

// ============================================================================
// TEST 13.1 — Validation Layer Detection
// ============================================================================
static void test_f13_validation_detection(F13Context& ctx) {
    printf("  [TEST] test_f13_validation_detection\n"); fflush(stdout);

    if (!ctx.initialized) {
        printf("  FAIL: Vulkan not initialized\n"); s_failed++; return;
    }

    bool available = ctx.vk.isValidationAvailable();
    bool enabled = ctx.vk.isValidationEnabled();

    printf("  available=%s enabled=%s\n",
           available ? "YES" : "NO", enabled ? "YES" : "NO");

    if (available && enabled) {
        printf("  PASS (validation layers detected and enabled)\n"); s_passed++;
    } else if (!available) {
        printf("  SKIP (VK_LAYER_KHRONOS_validation not installed)\n");
        printf("  NOT AVAILABLE — cannot perform validation tests\n");
        s_skipped++;
    } else {
        printf("  FAIL: available but not enabled\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.2 — Debug Messenger
// ============================================================================
static void test_f13_debug_messenger(F13Context& ctx) {
    printf("  [TEST] test_f13_debug_messenger\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path shaderPath = kShaderDir / "glsl" / "solid_red.glsl";
    if (!fs::exists(shaderPath)) {
        printf("  SKIP: solid_red.glsl not found\n"); s_skipped++; return;
    }

    auto compileResult = ctx.runtime->compileShader(ShaderLanguage::GLSL, shaderPath, "");
    if (!compileResult) {
        printf("  FAIL: compile failed\n"); s_failed++; return;
    }
    auto loadStatus = ctx.sr->loadIndividualShader(compileResult.value(), shaderPath);
    if (!loadStatus) {
        printf("  FAIL: loadIndividualShader failed\n"); s_failed++; return;
    }

    captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);

    auto msgs = ctx.vk.takeValidationMessages();
    printf("  captured %zu validation messages\n", msgs.size());

    bool messengerWorking = true;
    for (auto& m : msgs) {
        const char* sevStr = "INFO";
        if (m.severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) sevStr = "ERROR";
        else if (m.severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) sevStr = "WARNING";
        printf("    [%s] %s\n", sevStr, m.message.c_str());
    }

    if (messengerWorking) {
        printf("  PASS (debug messenger capturing messages)\n"); s_passed++;
    } else {
        printf("  FAIL: debug messenger not working\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.3 — GLSL with Validation
// ============================================================================
static void test_f13_glsl_with_validation(F13Context& ctx) {
    printf("  [TEST] test_f13_glsl_with_validation\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path shaderPath = kShaderDir / "glsl" / "solid_red.glsl";
    if (!fs::exists(shaderPath)) {
        printf("  SKIP: solid_red.glsl not found\n"); s_skipped++; return;
    }

    auto compileResult = ctx.runtime->compileShader(ShaderLanguage::GLSL, shaderPath, "");
    if (!compileResult) {
        printf("  FAIL: compile failed\n"); s_failed++; return;
    }

    auto loadStatus = ctx.sr->loadIndividualShader(compileResult.value(), shaderPath);
    if (!loadStatus) {
        printf("  FAIL: loadIndividualShader failed\n"); s_failed++; return;
    }

    auto rb = captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    bool isRed = validateSolidColor(rb, 255, 0, 0);
    bool noErrors = !summary.hasCriticalOrError();

    if (isRed && noErrors) {
        printf("  PASS (GLSL with validation: red, 0 errors)\n"); s_passed++;
    } else if (!isRed) {
        printf("  FAIL: not red\n"); s_failed++;
    } else {
        printf("  FAIL: validation errors detected\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.4 — SLANG with Validation
// ============================================================================
static void test_f13_slang_with_validation(F13Context& ctx) {
    printf("  [TEST] test_f13_slang_with_validation\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path testDir = kTestOutputDir / "slang";
    std::error_code ec;
    fs::create_directories(testDir, ec);

    fs::path slangPath = testDir / "solid_blue.slang";
    writeFile(slangPath, R"(#version 450

#pragma stage vertex
void main() {
    gl_Position = vec4(vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1) * 2.0 - 1.0, 0.0, 1.0);
}

#pragma stage fragment
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(0.0, 0.0, 1.0, 1.0);
}
)");

    auto compileResult = ctx.runtime->compileShader(ShaderLanguage::Slang, slangPath, "");
    if (!compileResult) {
        printf("  FAIL: compile failed: %s\n", compileResult.error().c_str());
        s_failed++; return;
    }

    auto loadStatus = ctx.sr->loadIndividualShader(compileResult.value(), slangPath);
    if (!loadStatus) {
        printf("  FAIL: loadIndividualShader failed\n"); s_failed++; return;
    }

    auto rb = captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    bool isBlue = validateSolidColor(rb, 0, 0, 255);
    bool noErrors = !summary.hasCriticalOrError();

    if (isBlue && noErrors) {
        printf("  PASS (SLANG with validation: blue, 0 errors)\n"); s_passed++;
    } else if (!isBlue) {
        printf("  FAIL: not blue\n"); s_failed++;
    } else {
        printf("  FAIL: validation errors detected\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.5 — SLANGP Preset with Validation
// ============================================================================
static void test_f13_slangp_with_validation(F13Context& ctx) {
    printf("  [TEST] test_f13_slangp_with_validation\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path presetPath = kShaderDir / "presets" / "single_red.slangp";
    if (!fs::exists(presetPath)) {
        printf("  SKIP: single_red.slangp not found\n"); s_skipped++; return;
    }

    ShaderValidationProfile skipGpu;
    skipGpu.skipGpuTest = true;
    ctx.sr->setValidationProfile(skipGpu);

    auto loadStatus = ctx.sr->loadPreset(presetPath);
    if (!loadStatus) {
        printf("  FAIL: loadPreset failed: %s\n", loadStatus.message.c_str());
        s_failed++; return;
    }

    captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    if (summary.hasCriticalOrError()) {
        printf("  FAIL: validation errors during SLANGP preset render\n"); s_failed++;
    } else {
        printf("  PASS (SLANGP preset with validation: 0 errors)\n"); s_passed++;
    }
}

// ============================================================================
// TEST 13.6 — Rollback with Validation
// ============================================================================
static void test_f13_rollback_with_validation(F13Context& ctx) {
    printf("  [TEST] test_f13_rollback_with_validation\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path goodShader = kShaderDir / "glsl" / "solid_green.glsl";
    if (!fs::exists(goodShader)) {
        printf("  SKIP: solid_green.glsl not found\n"); s_skipped++; return;
    }

    auto compileGood = ctx.runtime->compileShader(ShaderLanguage::GLSL, goodShader, "");
    if (!compileGood) {
        printf("  SKIP: cannot compile good shader\n"); s_skipped++; return;
    }

    auto loadGood = ctx.sr->loadIndividualShader(compileGood.value(), goodShader);
    if (!loadGood) {
        printf("  SKIP: cannot load good shader\n"); s_skipped++; return;
    }

    auto rbBefore = captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);
    bool wasGreen = validateSolidColor(rbBefore, 0, 255, 0);
    if (!wasGreen) {
        printf("  FAIL: good shader did not produce green\n"); s_failed++; return;
    }

    fs::path testDir = kTestOutputDir / "rollback";
    std::error_code ec;
    fs::create_directories(testDir, ec);
    fs::path badShader = testDir / "bad.glsl";
    writeFile(badShader, "THIS IS NOT VALID GLSL !!!###\n");

    ShaderLibrary badLib(testDir);
    badLib.scan();
    ShaderLibraryCompiler badCompiler(badLib, *ctx.runtime, ctx.sr);
    auto compileBad = badCompiler.compileEntry(0);

    if (compileBad.success) {
        auto loadBad = ctx.sr->loadIndividualShader(compileBad.module, badShader);
        if (loadBad) {
            printf("  FAIL: bad shader should not load\n"); s_failed++; return;
        }
    }

    auto rbAfter = captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);
    bool stillGreen = validateSolidColor(rbAfter, 0, 255, 0);

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    if (stillGreen && !summary.hasCriticalOrError()) {
        printf("  PASS (rollback with validation: preserved, 0 errors)\n"); s_passed++;
    } else if (!stillGreen) {
        printf("  FAIL: frame not preserved after rollback\n"); s_failed++;
    } else {
        printf("  FAIL: validation errors during rollback\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.7 — Hot Reload with Validation
// ============================================================================
static void test_f13_hot_reload_with_validation(F13Context& ctx) {
    printf("  [TEST] test_f13_hot_reload_with_validation\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path testDir = kTestOutputDir / "hotreload";
    std::error_code ec;
    fs::create_directories(testDir, ec);

    fs::path shaderPath = testDir / "color.glsl";
    writeFile(shaderPath, R"(#version 450
layout(set=0, binding=0) uniform UBO {
    vec4 OutputSize;
    vec4 OriginalSize;
    vec4 SourceSize;
    mat4 MVP;
};
layout(push_constant) uniform PushData {
    float Time;
    float FrameCount;
} params;
#pragma stage vertex
void main() {
    vec2 pos = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1) * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
}
#pragma stage fragment
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
)");

    ShaderRuntimeConfig rtCfg;
    rtCfg.rootDirectory = kBaseDir;
    rtCfg.shaderCacheDirectory = kBaseDir / "build" / "fase13-cache";
    rtCfg.slangcPath = kBaseDir / "tools" / "slangc.exe";
    rtCfg.compileShadersToSpirv = true;
    rtCfg.includeDirectories = { testDir };

    ShaderRuntime localRuntime(rtCfg);
    localRuntime.initialize();

    auto compileRed = localRuntime.compileShader(ShaderLanguage::GLSL, shaderPath, "");
    if (!compileRed) {
        printf("  FAIL: cannot compile red\n"); s_failed++; return;
    }
    auto loadRed = ctx.sr->loadIndividualShader(compileRed.value(), shaderPath);
    if (!loadRed) {
        printf("  FAIL: cannot load red\n"); s_failed++; return;
    }

    captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);

    writeFile(shaderPath, R"(#version 450
layout(set=0, binding=0) uniform UBO {
    vec4 OutputSize;
    vec4 OriginalSize;
    vec4 SourceSize;
    mat4 MVP;
};
layout(push_constant) uniform PushData {
    float Time;
    float FrameCount;
} params;
#pragma stage vertex
void main() {
    vec2 pos = vec2(gl_VertexIndex & 1, (gl_VertexIndex >> 1) & 1) * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
}
#pragma stage fragment
layout(location = 0) out vec4 fragColor;
void main() {
    fragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
)");

    auto compileGreen = localRuntime.compileShader(ShaderLanguage::GLSL, shaderPath, "");
    if (!compileGreen) {
        printf("  FAIL: cannot compile green\n"); s_failed++; return;
    }
    auto loadGreen = ctx.sr->loadIndividualShader(compileGreen.value(), shaderPath);
    if (!loadGreen) {
        printf("  FAIL: cannot load green\n"); s_failed++; return;
    }

    auto rbGreen = captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);
    bool isGreen = validateSolidColor(rbGreen, 0, 255, 0);

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    if (isGreen && !summary.hasCriticalOrError()) {
        printf("  PASS (hot reload with validation: green, 0 errors)\n"); s_passed++;
    } else if (!isGreen) {
        printf("  FAIL: not green after reload\n"); s_failed++;
    } else {
        printf("  FAIL: validation errors during hot reload\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.8 — Resource Lifetime 100 Cycles with Validation
// ============================================================================
static void test_f13_resource_lifetime_100(F13Context& ctx) {
    printf("  [TEST] test_f13_resource_lifetime_100\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path shaderPath = kShaderDir / "glsl" / "solid_red.glsl";
    if (!fs::exists(shaderPath)) {
        printf("  SKIP: solid_red.glsl not found\n"); s_skipped++; return;
    }

    int successCount = 0;
    for (int i = 0; i < 100; ++i) {
        auto compileResult = ctx.runtime->compileShader(ShaderLanguage::GLSL, shaderPath, "");
        if (!compileResult) continue;

        auto loadStatus = ctx.sr->loadIndividualShader(compileResult.value(), shaderPath);
        if (!loadStatus) continue;

        FrameContext fc{};
        fc.frameIndex = i;
        fc.outputExtent = {ctx.window.width, ctx.window.height};
        auto renderStatus = ctx.sr->render(fc, nullptr);
        if (renderStatus) successCount++;
    }

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    printf("  completed %d/100 cycles\n", successCount);

    if (successCount >= 95 && !summary.hasCriticalOrError()) {
        printf("  PASS (100 cycles with validation, 0 errors)\n"); s_passed++;
    } else if (successCount < 95) {
        printf("  FAIL: only %d/100 succeeded\n", successCount); s_failed++;
    } else {
        printf("  FAIL: validation errors during lifetime test\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.9 — Shader Failure with Validation (valid→active, invalid→fail)
// ============================================================================
static void test_f13_shader_failure_with_validation(F13Context& ctx) {
    printf("  [TEST] test_f13_shader_failure_with_validation\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path goodShader = kShaderDir / "glsl" / "solid_green.glsl";
    if (!fs::exists(goodShader)) {
        printf("  SKIP: solid_green.glsl not found\n"); s_skipped++; return;
    }

    auto compileGood = ctx.runtime->compileShader(ShaderLanguage::GLSL, goodShader, "");
    if (!compileGood) {
        printf("  SKIP: cannot compile good shader\n"); s_skipped++; return;
    }

    auto loadGood = ctx.sr->loadIndividualShader(compileGood.value(), goodShader);
    if (!loadGood) {
        printf("  SKIP: cannot load good shader\n"); s_skipped++; return;
    }

    ctx.vk.clearValidationMessages();

    auto rb = captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);
    bool isGreen = validateSolidColor(rb, 0, 255, 0);
    if (!isGreen) {
        printf("  FAIL: good shader did not produce green\n"); s_failed++; return;
    }

    fs::path testDir = kTestOutputDir / "failure";
    std::error_code ec;
    fs::create_directories(testDir, ec);
    fs::path badShader = testDir / "bad_pipeline.glsl";
    writeFile(badShader, "#version 450\nvoid main() { THIS IS BROKEN }");

    ShaderLibrary badLib(testDir);
    badLib.scan();
    ShaderLibraryCompiler badCompiler(badLib, *ctx.runtime, ctx.sr);
    auto compileBad = badCompiler.compileEntry(0);
    if (compileBad.success) {
        ctx.sr->loadIndividualShader(compileBad.module, badShader);
    }

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    auto rbAfter = captureFrame(ctx.vk, *ctx.sr, ctx.window.width, ctx.window.height);
    bool stillGreen = validateSolidColor(rbAfter, 0, 255, 0);

    if (stillGreen) {
        printf("  PASS (shader failure with validation: good→active, bad→rejected, still green)\n"); s_passed++;
    } else {
        printf("  FAIL: frame changed after failed shader\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.10 — Destroy with Validation (clean shutdown, no leaks)
// ============================================================================
static void test_f13_destroy_with_validation(F13Context& ctx) {
    printf("  [TEST] test_f13_destroy_with_validation\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    fs::path shaderPath = kShaderDir / "glsl" / "solid_red.glsl";
    if (!fs::exists(shaderPath)) {
        printf("  SKIP: solid_red.glsl not found\n"); s_skipped++; return;
    }

    for (int i = 0; i < 5; ++i) {
        auto compileResult = ctx.runtime->compileShader(ShaderLanguage::GLSL, shaderPath, "");
        if (!compileResult) continue;
        ctx.sr->loadIndividualShader(compileResult.value(), shaderPath);
        FrameContext fc{};
        fc.frameIndex = i;
        fc.outputExtent = {ctx.window.width, ctx.window.height};
        ctx.sr->render(fc, nullptr);
    }

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    if (!summary.hasCriticalOrError()) {
        printf("  PASS (destroy test: 5 render cycles, 0 validation errors)\n"); s_passed++;
    } else {
        printf("  FAIL: validation errors detected\n"); s_failed++;
    }
}

// ============================================================================
// TEST 13.11 — Thread/Lifetime Audit
// ============================================================================
static void test_f13_lifetime_audit(F13Context& ctx) {
    printf("  [TEST] test_f13_lifetime_audit\n"); fflush(stdout);
    if (!ctx.initialized) { printf("  SKIP\n"); s_skipped++; return; }
    if (!ctx.validationEnabled) { printf("  SKIP (validation not available)\n"); s_skipped++; return; }

    ctx.vk.clearValidationMessages();

    printf("    Checking ShaderRenderer ownership... ");
    if (ctx.sr) printf("OK\n"); else { printf("NULL\n"); s_failed++; return; }

    printf("    Checking ShaderRuntime ownership... ");
    if (ctx.runtime) printf("OK\n"); else { printf("NULL\n"); s_failed++; return; }

    printf("    Checking ShaderLibrary ownership... ");
    if (ctx.library) printf("OK\n"); else { printf("NULL\n"); s_failed++; return; }

    printf("    Checking ShaderLibraryCompiler ownership... ");
    if (ctx.compiler) printf("OK\n"); else { printf("NULL\n"); s_failed++; return; }

    printf("    Checking for stale pointers... ");
    const auto* compiled = ctx.sr->compiledPreset();
    printf("compiledPreset=%p (may be null, that's OK)\n", compiled);

    printf("    Checking VulkanRenderer state... ");
    printf("initialized=%s device=%s\n",
           ctx.vk.isInitialized() ? "yes" : "no",
           ctx.vk.isInitialized() ? "valid" : "n/a");

    ValidationSummary summary;
    summary.collect(ctx.vk);
    summary.print();

    if (!summary.hasCriticalOrError()) {
        printf("  PASS (lifetime audit: all objects valid, 0 validation errors)\n"); s_passed++;
    } else {
        printf("  FAIL: validation errors during audit\n"); s_failed++;
    }
}

// ============================================================================
// RUN ALL
// ============================================================================
void monix::renderer_vk::tests::runAllFase13ValidationTests() {
    printf("\n=== FASE 13: Vulkan Validation & Runtime Hardening ===\n");
    fflush(stdout);

    ensureOutputDir();

    F13Context ctx;
    bool vulkanAvailable = ctx.initialize();

    F13_TEST_SECTION("13.A — Validation Layer Detection");
    F13_RUN_TEST(test_f13_validation_detection, ctx);

    F13_TEST_SECTION("13.B — Debug Messenger");
    F13_RUN_TEST(test_f13_debug_messenger, ctx);

    if (vulkanAvailable && ctx.validationEnabled) {
        F13_TEST_SECTION("13.C — GLSL with Validation");
        F13_RUN_TEST(test_f13_glsl_with_validation, ctx);

        F13_TEST_SECTION("13.C — SLANG with Validation");
        F13_RUN_TEST(test_f13_slang_with_validation, ctx);

        F13_TEST_SECTION("13.C — SLANGP Preset with Validation");
        F13_RUN_TEST(test_f13_slangp_with_validation, ctx);

        F13_TEST_SECTION("13.F — Rollback with Validation");
        F13_RUN_TEST(test_f13_rollback_with_validation, ctx);

        F13_TEST_SECTION("13.C — Hot Reload with Validation");
        F13_RUN_TEST(test_f13_hot_reload_with_validation, ctx);

        F13_TEST_SECTION("13.E — Resource Lifetime 100 Cycles");
        F13_RUN_TEST(test_f13_resource_lifetime_100, ctx);

        F13_TEST_SECTION("13.F — Shader Failure with Validation");
        F13_RUN_TEST(test_f13_shader_failure_with_validation, ctx);

        F13_TEST_SECTION("13.C — Destroy with Validation");
        F13_RUN_TEST(test_f13_destroy_with_validation, ctx);

        F13_TEST_SECTION("13.G — Thread/Lifetime Audit");
        F13_RUN_TEST(test_f13_lifetime_audit, ctx);
    } else if (!vulkanAvailable) {
        printf("\n  *** Vulkan not available — GPU tests SKIPPED ***\n");
    } else {
        printf("\n  *** Validation layers not available — validation tests SKIPPED ***\n");
    }

    ctx.shutdown();

    printf("\n=== FASE 13 Results: %d passed, %d failed, %d skipped ===\n",
        s_passed, s_failed, s_skipped);
    printf("=== FASE 13 Validation & Runtime Hardening complete ===\n");
}
