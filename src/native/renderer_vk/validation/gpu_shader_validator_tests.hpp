#pragma once

#include "../validation/GpuShaderValidator.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>

namespace monix::renderer_vk::tests {

inline void test_analyze_empty_data() {
    GpuShaderValidator v;
    std::vector<uint8_t> empty;
    auto stats = v.analyzeOutput(empty, 0, 0);
    assert(stats.totalPixels == 0);
    assert(stats.nonZeroPixels == 0);
    printf("  [PASS] analyze_empty_data\n");
}

inline void test_analyze_all_zero() {
    GpuShaderValidator v;
    std::vector<uint8_t> data(256 * 4, 0);
    auto stats = v.analyzeOutput(data, 256, 1);
    assert(stats.totalPixels == 256);
    assert(stats.nonZeroPixels == 0);
    assert(stats.meanR == 0.0);
    assert(stats.meanG == 0.0);
    assert(stats.meanB == 0.0);
    printf("  [PASS] analyze_all_zero\n");
}

inline void test_analyze_all_white() {
    GpuShaderValidator v;
    std::vector<uint8_t> data(16 * 16 * 4);
    for (size_t i = 0; i < data.size(); i += 4) {
        data[i + 0] = 255;
        data[i + 1] = 255;
        data[i + 2] = 255;
        data[i + 3] = 255;
    }
    auto stats = v.analyzeOutput(data, 16, 16);
    assert(stats.totalPixels == 256);
    assert(stats.nonZeroPixels == 256);
    assert(stats.meanR > 0.99);
    assert(stats.meanLuminance > 0.99);
    assert(stats.uniqueColors == 1);
    printf("  [PASS] analyze_all_white\n");
}

inline void test_analyze_gradient() {
    GpuShaderValidator v;
    uint32_t w = 8, h = 8;
    std::vector<uint8_t> data(w * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            size_t idx = (y * w + x) * 4;
            data[idx + 0] = static_cast<uint8_t>((x * 255) / w);
            data[idx + 1] = static_cast<uint8_t>((y * 255) / h);
            data[idx + 2] = 128;
            data[idx + 3] = 255;
        }
    }
    auto stats = v.analyzeOutput(data, w, h);
    assert(stats.totalPixels == 64);
    assert(stats.nonZeroPixels == 64);
    assert(stats.uniqueColors > 1);
    assert(stats.maxR > stats.minR || stats.maxG > stats.minG);
    printf("  [PASS] analyze_gradient\n");
}

inline void test_validate_black_rejected() {
    GpuShaderValidator v;
    v.setProfile(ShaderValidationProfile{});
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 0;
    stats.meanLuminance = 0.0;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::FailBlackOutput);
    printf("  [PASS] validate_black_rejected\n");
}

inline void test_validate_black_allowed() {
    GpuShaderValidator v;
    ShaderValidationProfile p;
    p.allowBlackOutput = true;
    v.setProfile(p);
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 0;
    stats.meanLuminance = 0.0;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::Pass);
    printf("  [PASS] validate_black_allowed\n");
}

inline void test_validate_constant_rejected() {
    GpuShaderValidator v;
    v.setProfile(ShaderValidationProfile{});
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 100;
    stats.meanLuminance = 0.5;
    stats.uniqueColors = 1;
    stats.meanR = 0.5;
    stats.meanG = 0.5;
    stats.meanB = 0.5;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::FailConstantOutput);
    printf("  [PASS] validate_constant_rejected\n");
}

inline void test_validate_constant_allowed_non_final() {
    GpuShaderValidator v;
    v.setProfile(ShaderValidationProfile{});
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 100;
    stats.meanLuminance = 0.5;
    stats.uniqueColors = 1;
    stats.meanR = 0.5;
    stats.meanG = 0.5;
    stats.meanB = 0.5;
    auto result = v.validateOutput(stats, false);
    assert(result == GpuTestResult::Pass);
    printf("  [PASS] validate_constant_allowed_non_final\n");
}

inline void test_validate_nan_rejected() {
    GpuShaderValidator v;
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.hasNaN = true;
    auto result = v.validateOutput(stats, false);
    assert(result == GpuTestResult::FailNaN);
    printf("  [PASS] validate_nan_rejected\n");
}

inline void test_validate_inf_rejected() {
    GpuShaderValidator v;
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.hasInf = true;
    auto result = v.validateOutput(stats, false);
    assert(result == GpuTestResult::FailInf);
    printf("  [PASS] validate_inf_rejected\n");
}

inline void test_validate_no_data() {
    GpuShaderValidator v;
    OutputStatistics stats{};
    auto result = v.validateOutput(stats, false);
    assert(result == GpuTestResult::FailReadbackFailed);
    printf("  [PASS] validate_no_data\n");
}

inline void test_permissive_allows_black() {
    auto v = GpuShaderValidator::makePermissive();
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 0;
    stats.meanLuminance = 0.0;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::Pass);
    printf("  [PASS] permissive_allows_black\n");
}

inline void test_strict_rejects_black() {
    auto v = GpuShaderValidator::makeStrict();
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 0;
    stats.meanLuminance = 0.0;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::FailBlackOutput);
    printf("  [PASS] strict_rejects_black\n");
}

inline void test_diagnostic_format_pass() {
    GpuTestDiagnostic diag;
    diag.result = GpuTestResult::Pass;
    diag.presetName = "test.slangp";
    diag.passIndex = 0;
    auto str = diag.format();
    assert(str.find("PASS") != std::string::npos);
    assert(str.find("test.slangp") != std::string::npos);
    assert(str.find("Candidate accepted") != std::string::npos);
    printf("  [PASS] diagnostic_format_pass\n");
}

inline void test_diagnostic_format_fail() {
    GpuTestDiagnostic diag;
    diag.result = GpuTestResult::FailBlackOutput;
    diag.presetName = "bad.slangp";
    diag.passIndex = 1;
    diag.detail = "NonZeroPixels: 0/100 (0%)\nMeanLuminance: 0";
    auto str = diag.format();
    assert(str.find("OUTPUT_BLACK") != std::string::npos);
    assert(str.find("bad.slangp") != std::string::npos);
    assert(str.find("Candidate rejected") != std::string::npos);
    assert(str.find("NonZeroPixels") != std::string::npos);
    printf("  [PASS] diagnostic_format_fail\n");
}

inline void test_gpu_test_result_names() {
    assert(std::string(gpuTestResultName(GpuTestResult::Pass)) == "PASS");
    assert(std::string(gpuTestResultName(GpuTestResult::FailBlackOutput)) == "OUTPUT_BLACK");
    assert(std::string(gpuTestResultName(GpuTestResult::FailConstantOutput)) == "OUTPUT_CONSTANT");
    assert(std::string(gpuTestResultName(GpuTestResult::FailNaN)) == "OUTPUT_INVALID_FLOAT");
    assert(std::string(gpuTestResultName(GpuTestResult::FailInf)) == "OUTPUT_INVALID_FLOAT");
    assert(std::string(gpuTestResultName(GpuTestResult::FailLowContent)) == "OUTPUT_LOW_CONTENT");
    assert(std::string(gpuTestResultName(GpuTestResult::FailReadbackFailed)) == "OUTPUT_READBACK_FAILED");
    assert(std::string(gpuTestResultName(GpuTestResult::FailRenderFailed)) == "GPU_RENDER_FAILED");
    assert(std::string(gpuTestResultName(GpuTestResult::FailNoRenderer)) == "NO_RENDERER");
    assert(std::string(gpuTestResultName(GpuTestResult::FailNotLoaded)) == "GPU_NOT_LOADED");
    assert(std::string(gpuTestResultName(GpuTestResult::FailUnsupported)) == "UNSUPPORTED");
    assert(std::string(gpuTestResultName(GpuTestResult::Skip)) == "SKIPPED");
    printf("  [PASS] gpu_test_result_names\n");
}

inline void test_set_get_profile() {
    GpuShaderValidator v;
    ShaderValidationProfile p;
    p.allowBlackOutput = true;
    p.minimumNonZeroPixelRatio = 0.1;
    p.testWidth = 512;
    v.setProfile(p);
    assert(v.profile().allowBlackOutput == true);
    assert(v.profile().minimumNonZeroPixelRatio == 0.1);
    assert(v.profile().testWidth == 512);
    printf("  [PASS] set_get_profile\n");
}

inline void test_run_gpu_test_no_data() {
    GpuShaderValidator v;
    auto diag = v.runGpuTest();
    assert(diag.result == GpuTestResult::FailReadbackFailed);
    assert(diag.detail.find("No readback data") != std::string::npos);
    printf("  [PASS] run_gpu_test_no_data\n");
}

inline void test_run_gpu_test_with_data() {
    GpuShaderValidator v;
    uint32_t w = 4, h = 4;
    std::vector<uint8_t> data(w * h * 4);
    for (size_t i = 0; i < data.size(); i += 4) {
        data[i + 0] = 200;
        data[i + 1] = 200;
        data[i + 2] = 200;
        data[i + 3] = 255;
    }
    v.setGpuTestData(data, w, h, "test.slangp", -1);
    auto diag = v.runGpuTest();
    assert(diag.result == GpuTestResult::Pass);
    assert(diag.presetName == "test.slangp");
    assert(diag.passIndex == -1);
    assert(diag.stats.nonZeroPixels == 16);
    assert(diag.stats.uniqueColors == 1);
    printf("  [PASS] run_gpu_test_with_data\n");
}

inline void test_run_gpu_test_gradient_rejects_constant() {
    ShaderValidationProfile p;
    p.allowConstantOutput = false;
    GpuShaderValidator v;
    v.setProfile(p);
    uint32_t w = 4, h = 4;
    std::vector<uint8_t> data(w * h * 4);
    for (size_t i = 0; i < data.size(); i += 4) {
        data[i + 0] = 200;
        data[i + 1] = 200;
        data[i + 2] = 200;
        data[i + 3] = 255;
    }
    v.setGpuTestData(data, w, h, "constant.slangp", 0);
    auto diag = v.runGpuTest();
    assert(diag.result == GpuTestResult::FailConstantOutput);
    printf("  [PASS] run_gpu_test_gradient_rejects_constant\n");
}

inline void test_analyze_pixel_formats() {
    GpuShaderValidator v;
    std::vector<uint8_t> data(4 * 4 * 4);
    for (uint32_t i = 0; i < 16; ++i) {
        data[i * 4 + 0] = 255;
        data[i * 4 + 1] = 128;
        data[i * 4 + 2] = 0;
        data[i * 4 + 3] = 255;
    }
    auto stats = v.analyzeOutput(data, 4, 4);
    assert(stats.meanB > 0.99);
    assert(stats.meanG > 0.49 && stats.meanG < 0.51);
    assert(stats.meanR < 0.01);
    assert(stats.meanA > 0.99);
    printf("  [PASS] analyze_pixel_formats\n");
}

inline void test_profile_minimums() {
    GpuShaderValidator v;
    ShaderValidationProfile p;
    p.minimumNonZeroPixelRatio = 0.5;
    p.minimumLuminance = 0.3;
    v.setProfile(p);
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 40;
    stats.meanLuminance = 0.2;
    stats.meanR = 0.2;
    stats.meanG = 0.2;
    stats.meanB = 0.2;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::FailBlackOutput);
    printf("  [PASS] profile_minimums\n");
}

inline void test_profile_minimums_just_passes() {
    GpuShaderValidator v;
    ShaderValidationProfile p;
    p.minimumNonZeroPixelRatio = 0.5;
    p.minimumLuminance = 0.3;
    v.setProfile(p);
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 50;
    stats.meanLuminance = 0.3;
    stats.meanR = 0.3;
    stats.meanG = 0.3;
    stats.meanB = 0.3;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::Pass);
    printf("  [PASS] profile_minimums_just_passes\n");
}

inline void test_constant_output_allow() {
    ShaderValidationProfile p;
    p.allowConstantOutput = true;
    GpuShaderValidator v;
    v.setProfile(p);
    OutputStatistics stats{};
    stats.totalPixels = 100;
    stats.nonZeroPixels = 100;
    stats.uniqueColors = 1;
    stats.meanLuminance = 0.5;
    stats.meanR = 0.5;
    stats.meanG = 0.5;
    stats.meanB = 0.5;
    auto result = v.validateOutput(stats, true);
    assert(result == GpuTestResult::Pass);
    printf("  [PASS] constant_output_allow\n");
}

inline void test_diagnostic_skip() {
    GpuTestDiagnostic diag;
    diag.result = GpuTestResult::Skip;
    diag.presetName = "skipped.slangp";
    auto str = diag.format();
    assert(str.find("SKIPPED") != std::string::npos);
    assert(str.find("Candidate accepted") != std::string::npos);
    printf("  [PASS] diagnostic_skip\n");
}

inline void test_unique_colors_counting() {
    GpuShaderValidator v;
    std::vector<uint8_t> data(2 * 1 * 4);
    data[0] = 255; data[1] = 0; data[2] = 0; data[3] = 255;
    data[4] = 128; data[5] = 0; data[6] = 0; data[7] = 255;
    auto stats = v.analyzeOutput(data, 2, 1);
    assert(stats.uniqueColors == 2);
    assert(stats.nonZeroPixels == 2);
    printf("  [PASS] unique_colors_counting\n");
}

}  // namespace monix::renderer_vk::tests
